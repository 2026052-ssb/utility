#include "rviz_capture/capture_panel.hpp"
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
void CapturePanel::capture()
{
  QStringList written;
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
          if (display->getClassId() != "rviz_default_plugins/Image" || !display->isEnabled()) {continue;}
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
    QFile manifest(QDir(session_).filePath("frames.jsonl"));
    const auto line = QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Append) ||
      manifest.write(line) != line.size() || !manifest.flush()) {
      throw std::runtime_error("Cannot write capture manifest");
    }
    ++frame_;
    status_->setText(QString("Saved %1 frame(s)\n%2").arg(frame_).arg(session_));
  } catch (const std::exception & error) {
    for (const auto & path : written) {QFile::remove(path);}
    stop();
    status_->setText(QString("Capture stopped: %1").arg(error.what()));
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
