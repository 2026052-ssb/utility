#ifndef RVIZ_CAPTURE__CAPTURE_PANEL_HPP_
#define RVIZ_CAPTURE__CAPTURE_PANEL_HPP_
#include <rviz_common/panel.hpp>
#include <QString>
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QTimer;
namespace rviz_capture
{
class CapturePanel : public rviz_common::Panel
{
  Q_OBJECT
public:
  explicit CapturePanel(QWidget * parent = nullptr);
  void load(const rviz_common::Config & config) override;
  void save(rviz_common::Config config) const override;
private:
  void capture();
  void start();
  void stop();
  void newSession();
  void setRecording(bool recording);
  QCheckBox * view_;
  QCheckBox * images_;
  QCheckBox * window_;
  QLineEdit * output_;
  QSpinBox * fps_;
  QPushButton * record_;
  QPushButton * single_;
  QPushButton * browse_;
  QLabel * status_;
  QTimer * timer_;
  QString session_;
  quint64 frame_ = 0;
};
}
#endif
