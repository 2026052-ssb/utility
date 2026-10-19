#include "rviz_capture/capture_panel.hpp"
#include "rviz_capture/synced_displays.hpp"
#include <cmath>
#include <set>
#include <rviz_common/ros_integration/ros_node_abstraction_iface.hpp>
#include <array>
#include <functional>
#include <stdexcept>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QPainter>
#include <QTemporaryDir>
#include <QSpinBox>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <rviz_common/display.hpp>
#include <rviz_common/display_context.hpp>
#include <rviz_common/display_group.hpp>
#include <rviz_common/render_panel.hpp>
#include <rviz_common/view_manager.hpp>
#include <rviz_rendering/render_window.hpp>
#include <pluginlib/class_list_macros.hpp>

namespace rviz_capture
{
CapturePanel::CapturePanel(QWidget * parent) : Panel(parent)
{
  auto * layout = new QVBoxLayout(this);
  view_ = new QCheckBox("3D view", this);
  images_ = new QCheckBox("Image panels (each separately)", this);
  window_ = new QCheckBox("Whole RViz window", this);
  view_->setChecked(true);
  images_->setChecked(true);
  layout->addWidget(view_);
  layout->addWidget(images_);
  layout->addWidget(window_);
  output_ = new QLineEdit(QDir::homePath() + "/rviz_captures", this);
  browse_ = new QPushButton("Choose output directory", this);
  fps_ = new QSpinBox(this);
  fps_->setRange(1, 60);
  fps_->setValue(10);
  auto * form = new QFormLayout();
  form->addRow("Output", output_);
  form->addRow("Target FPS", fps_);
  layout->addLayout(form);
  layout->addWidget(browse_);
  single_ = new QPushButton("Capture frame", this);
  record_ = new QPushButton("Start recording", this);
  layout->addWidget(single_);
  layout->addWidget(record_);
  status_ = new QLabel("Ready", this);
  status_->setWordWrap(true);
  layout->addWidget(status_);
  request_timer_ = new QTimer(this);
  connect(request_timer_, &QTimer::timeout, this, &CapturePanel::pollRequest);
  timer_ = new QTimer(this);
  timer_->setTimerType(Qt::PreciseTimer);
  connect(timer_, &QTimer::timeout, this, &CapturePanel::capture);
  connect(single_, &QPushButton::clicked, this, &CapturePanel::capture);
  connect(record_, &QPushButton::clicked, this, [this] {
    if (timer_->isActive()) {stop();} else {start();}
  });
  connect(browse_, &QPushButton::clicked, this, [this] {
    const auto dir = QFileDialog::getExistingDirectory(this, "Capture output", output_->text());
    if (!dir.isEmpty()) {output_->setText(dir);}
  });
  const auto changed = [this] {session_.clear(); Q_EMIT configChanged();};
  connect(output_, &QLineEdit::textChanged, this, changed);
  connect(fps_, qOverload<int>(&QSpinBox::valueChanged), this, changed);
  for (auto * box : {view_, images_, window_}) {
    connect(box, &QCheckBox::toggled, this, changed);
  }
}
void CapturePanel::setRecording(bool recording)
{
  for (auto * widget : std::array<QWidget *, 7>{view_, images_, window_, output_,
      fps_, browse_, single_}) {widget->setEnabled(!recording);}
  record_->setText(recording ? "Stop recording" : "Start recording");
}
void CapturePanel::newSession()
{
  if (output_->text().trimmed().isEmpty()) {throw std::runtime_error("Choose an output directory");}
  session_ = QDir(output_->text()).absoluteFilePath(
    QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzzZ") + "_" +
    QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
  if (!QDir().mkpath(session_)) {session_.clear(); throw std::runtime_error("Cannot create output directory");}
  frame_ = 0;
}
void CapturePanel::start()
{
  session_.clear();
  timer_->start(qMax(1, qRound(1000.0 / fps_->value())));
  setRecording(true);
  capture();
}
void CapturePanel::stop()
{
  timer_->stop();
  setRecording(false);
}
bool CapturePanel::capture()
{
  QStringList written;
  last_files_.clear();
  try {
    if (!view_->isChecked() && !images_->isChecked() && !window_->isChecked()) {
      throw std::runtime_error("Select at least one capture target");
    }
    if (session_.isEmpty()) {newSession();}
    const auto stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const auto filename = QString("%1.png").arg(frame_, 6, 10, QChar('0'));
    QJsonArray files;
    const auto pathFor = [&](const QString & stream) {
      const auto dir = QDir(session_).filePath(stream);
      if (!QDir().mkpath(dir)) {throw std::runtime_error("Cannot create stream directory");}
      const auto path = QDir(dir).filePath(filename);
      if (QFileInfo::exists(path)) {throw std::runtime_error("Refusing to overwrite an existing frame");}
      written.append(path);
      files.append(stream + "/" + filename);
      return path;
    };
    const auto render = [&](rviz_common::RenderPanel * panel, const QString & stream) {
      if (!panel || !panel->isVisible() || !panel->getRenderWindow()->isExposed()) {
        throw std::runtime_error("Capture target is hidden; show all selected views before capture");
      }
      auto * target = panel->getRenderWindow();
      const auto path = pathFor(stream);
      target->renderNow();
      target->captureScreenShot(path.toStdString());
      if (!QFileInfo::exists(path) || QFileInfo(path).size() == 0) {
        throw std::runtime_error("Render screenshot was not saved");
      }
    };
    if (view_->isChecked()) {
      render(getDisplayContext()->getViewManager()->getRenderPanel(), "view3d");
    }
    if (images_->isChecked()) {
      int count = 0;
      std::function<void(rviz_common::DisplayGroup *, QString)> visit;
      visit = [&](rviz_common::DisplayGroup * group, QString prefix) {
        for (int i = 0; i < group->numDisplays(); ++i) {
          auto * display = group->getDisplayAt(i);
          const auto id = prefix + QString::number(i) + "_";
          if (auto * child = dynamic_cast<rviz_common::DisplayGroup *>(display)) {
            visit(child, id);
          }
          if ((display->getClassId() != "rviz_default_plugins/Image" &&
            display->getClassId() != "rviz_capture/SyncedImage") || !display->isEnabled()) {continue;}
          auto * widget = display->getAssociatedWidget();
          auto * panel = qobject_cast<rviz_common::RenderPanel *>(widget);
          if (!panel && widget) {panel = widget->findChild<rviz_common::RenderPanel *>();}
          auto name = display->getName();
          name.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
          render(panel, "images/" + id + name);
          ++count;
        }
      };
      visit(getDisplayContext()->getRootDisplayGroup(), "");
      if (!count) {throw std::runtime_error("No enabled RViz Image displays found");}
    }
    if (window_->isChecked()) {
      auto * top = getDisplayContext()->getViewManager()->getRenderPanel()->window();
      if (top->isMinimized() || !top->isVisible()) {throw std::runtime_error("Show the RViz window first");}
      // Native Ogre child windows are absent from QWidget::grab on some compositors.
      // Compose their render buffers over Qt's capture of the surrounding UI.
      auto shot = top->grab();
      if (shot.isNull()) {throw std::runtime_error("Cannot capture RViz UI");}
      QTemporaryDir temporary;
      if (!temporary.isValid()) {throw std::runtime_error("Cannot create capture temporary directory");}
      QPainter painter(&shot);
      int index = 0;
      for (auto * panel : top->findChildren<rviz_common::RenderPanel *>()) {
        auto * target = panel->getRenderWindow();
        if (!panel->isVisible() || !target->isExposed()) {continue;}
        const auto path = temporary.filePath(QString::number(index++) + ".png");
        target->renderNow();
        target->captureScreenShot(path.toStdString());
        QImage image(path);
        if (image.isNull()) {throw std::runtime_error("Cannot capture an RViz render buffer");}
        const auto pos = top->mapFromGlobal(target->mapToGlobal(QPoint(0, 0)));
        painter.drawImage(QRect(pos, target->size()), image);
      }
      painter.end();
      if (!shot.save(pathFor("window"))) {
        throw std::runtime_error("Whole-window screenshot failed");
      }
    }
    QJsonObject entry{{"frame", static_cast<qint64>(frame_)}, {"utc", stamp},
      {"target_fps", fps_->value()}, {"files", files}};
    if (request_) {
      entry.insert("source_frame", QString::fromStdString(request_->source_frame));
      entry.insert("sensor_stamp", QJsonObject{{"sec", request_->sensor_stamp.sec},
        {"nanosec", static_cast<qint64>(request_->sensor_stamp.nanosec)}});
      QJsonArray required;
      for (const auto & name : request_->required_displays) {required.append(QString::fromStdString(name));}
      entry.insert("required_displays", required);
      entry.insert("synchronized", true);
    } else {entry.insert("synchronized", false);}
    QFile manifest(QDir(session_).filePath("frames.jsonl"));
    const auto line = QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Append) ||
      manifest.write(line) != line.size() || !manifest.flush()) {
      throw std::runtime_error("Cannot write capture manifest");
    }
    last_files_ = written;
    ++frame_;
    status_->setText(QString("Saved %1 frame(s)\n%2").arg(frame_).arg(session_));
    return true;
  } catch (const std::exception & error) {
    for (const auto & path : written) {QFile::remove(path);}
    stop();
    last_error_ = QString::fromUtf8(error.what());
    status_->setText(QString("Capture stopped: %1").arg(last_error_));
    return false;
  }
}

void CapturePanel::onInitialize()
{
  auto node = getDisplayContext()->getRosNodeAbstraction().lock()->get_raw_node();
  service_ = node->create_service<CaptureFrame>("/rviz_capture/capture_frame",
    [this](std::shared_ptr<rmw_request_id_t> header, CaptureFrame::Request::SharedPtr request) {
      QMetaObject::invokeMethod(this, [this, header, request] {
        const auto reject = [&](const std::string & message) {
          CaptureFrame::Response response;
          response.message = message;
          service_->send_response(*header, response);
        };
        if (request_ || timer_->isActive()) {reject("Capture busy; stop timer recording first"); return;}
        if (request->source_frame.empty() || request->required_displays.empty() ||
          !std::isfinite(request->timeout_seconds) || request->timeout_seconds <= 0 ||
          request->timeout_seconds > 120 || request->sensor_stamp.nanosec >= 1000000000u) {
          reject("Invalid capture request (timeout must be in (0, 120])"); return;
        }
        std::set<std::string> names(request->required_displays.begin(), request->required_displays.end());
        if (names.size() != request->required_displays.size()) {reject("Duplicate required display names"); return;}
        last_files_.clear();
        request_ = request;
        request_header_ = header;
        deadline_ = std::chrono::steady_clock::now() +
          std::chrono::milliseconds(static_cast<int>(request->timeout_seconds * 1000));
        if (request->new_session) {session_.clear();}
        setRecording(true);
        record_->setEnabled(false);
        status_->setText("Waiting for display updates...");
        request_timer_->start(10);
      }, Qt::QueuedConnection);
    });
}
void CapturePanel::finishRequest(bool success, const std::string & message)
{
  request_timer_->stop();
  CaptureFrame::Response response;
  response.success = success;
  response.message = message;
  response.session_directory = session_.toStdString();
  response.capture_frame = success ? frame_ - 1 : frame_;
  if (success) {
    for (const auto & file : last_files_) {response.files.push_back(file.toStdString());}
  }
  service_->send_response(*request_header_, response);
  request_.reset();
  request_header_.reset();
  setRecording(false);
  record_->setEnabled(true);
  if (!success) {status_->setText(QString::fromStdString(message));}
}
void CapturePanel::pollRequest()
{
  if (!request_) {return;}
  missing_.clear();
  std::vector<rviz_common::Display *> displays;
  std::function<void(rviz_common::DisplayGroup *)> visit = [&](rviz_common::DisplayGroup * group) {
    for (int i = 0; i < group->numDisplays(); ++i) {
      auto * display = group->getDisplayAt(i);
      displays.push_back(display);
      if (auto * child = dynamic_cast<rviz_common::DisplayGroup *>(display)) {visit(child);}
    }
  };
  visit(getDisplayContext()->getRootDisplayGroup());
  std::set<rviz_common::Display *> required;
  for (const auto & name : request_->required_displays) {
    rviz_common::Display * match = nullptr;
    for (auto * display : displays) {
      if (display->getName().toStdString() != name) {continue;}
      if (match) {finishRequest(false, "Ambiguous display name: " + name); return;}
      match = display;
    }
    auto * tracker = dynamic_cast<FrameTracker *>(match);
    if (!match || !match->isEnabled() || !tracker) {
      finishRequest(false, "Required display missing, disabled or not a Synced display: " + name); return;
    }
    required.insert(match);
    if (!tracker->applied(request_->sensor_stamp)) {missing_.append(QString::fromStdString(name));}
    for (int i = 0; i < match->numChildren(); ++i) {
      auto * status = dynamic_cast<rviz_common::properties::StatusProperty *>(match->childAt(i));
      if (status && status->getLevel() == rviz_common::properties::StatusProperty::Error) {
        missing_.append(QString::fromStdString(name) + " (display error)");
      }
    }
  }
  // Every active dynamic view must participate, otherwise the output could mix frames.
  for (auto * display : displays) {
    if (!display->isEnabled()) {continue;}
    const auto id = display->getClassId();
    const bool dynamic = dynamic_cast<FrameTracker *>(display) ||
      id == "rviz_default_plugins/PointCloud2" || id == "rviz_default_plugins/MarkerArray" ||
      id == "rviz_default_plugins/Image";
    if (dynamic && !required.count(display)) {
      finishRequest(false, "Active sensor display omitted from required_displays: " + display->getName().toStdString()); return;
    }
  }
  if (missing_.empty()) {
    const bool success = capture();
    finishRequest(success, success ? "Saved synchronized frame" : last_error_.toStdString());
  } else if (std::chrono::steady_clock::now() >= deadline_) {
    finishRequest(false, "Timed out waiting for display update: " + missing_.join(", ").toStdString());
  }
}

void CapturePanel::save(rviz_common::Config config) const
{
  Panel::save(config);
  config.mapSetValue("Output", output_->text());
  config.mapSetValue("FPS", fps_->value());
  config.mapSetValue("View3D", view_->isChecked());
  config.mapSetValue("Images", images_->isChecked());
  config.mapSetValue("Window", window_->isChecked());
}
void CapturePanel::load(const rviz_common::Config & config)
{
  stop();
  Panel::load(config);
  QString output;
  int fps;
  bool enabled;
  if (config.mapGetString("Output", &output)) {output_->setText(output);}
  if (config.mapGetInt("FPS", &fps)) {fps_->setValue(fps);}
  if (config.mapGetBool("View3D", &enabled)) {view_->setChecked(enabled);}
  if (config.mapGetBool("Images", &enabled)) {images_->setChecked(enabled);}
  if (config.mapGetBool("Window", &enabled)) {window_->setChecked(enabled);}
  session_.clear();
}
}
PLUGINLIB_EXPORT_CLASS(rviz_capture::CapturePanel, rviz_common::Panel)
