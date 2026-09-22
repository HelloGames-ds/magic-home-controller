#pragma once

#include <QString>

namespace elkbledom::config {

inline constexpr auto ApplicationName = "Magic Home Controller";
inline constexpr auto ApplicationDisplayName = "Magic Home Controller";
inline constexpr auto OrganizationName = "Magic-Home-GUI";
inline constexpr auto ApplicationId = "magichome.controller.v4";

inline const QString StyleSheet = QStringLiteral(R"qss(
* { font-family: 'Segoe UI', 'Arial', sans-serif; font-size: 13px; }
QMainWindow, QWidget { background-color: #1e1e24; color: #e0e0e6; }
QTabWidget::pane {
    border: 1px solid #2d2d38; border-radius: 12px;
    background-color: #23232b; top: -1px;
}
QTabBar::tab {
    background-color: #23232b; color: #a0a0b0;
    padding: 10px 16px; margin-right: 4px;
    border-top-left-radius: 8px; border-top-right-radius: 8px;
    border: 1px solid #2d2d38; border-bottom: none;
}
QTabBar::tab:selected {
    background-color: #7b2cbf; color: #ffffff; border-color: #7b2cbf;
}
QTabBar::tab:hover:!selected { background-color: #2f2f3a; color: #d0d0e0; }
QPushButton {
    background-color: #2b2b35; color: #e0e0e6;
    border: 1px solid #3a3a48; border-radius: 8px;
    padding: 8px 16px; min-height: 22px;
}
QPushButton:hover { background-color: #7b2cbf; border-color: #9d4edd; color: white; }
QPushButton:pressed { background-color: #5a189a; }
QPushButton:disabled { background-color: #262630; color: #555560; border-color: #2d2d38; }
QPushButton[accent="true"] {
    background-color: #7b2cbf; border-color: #9d4edd;
    color: white; font-weight: 600;
}
QPushButton[accent="true"]:hover { background-color: #9d4edd; border-color: #b070ff; }
QPushButton[danger="true"] {
    background-color: #6d1a30; border-color: #b3234a;
    color: white; font-weight: 600;
}
QPushButton[danger="true"]:hover { background-color: #b3234a; border-color: #d6336c; }
QPushButton[slot="true"] { border: 2px dashed #3a3a48; border-radius: 10px; padding: 0; }
QPushButton[slot="true"]:hover { border-color: #b070ff; }
QPushButton[slotActive="true"] { border: 3px solid #b070ff; border-radius: 10px; padding: 0; }
QSlider::groove:horizontal { background: #2b2b35; height: 8px; border-radius: 4px; }
QSlider::sub-page:horizontal { background: #7b2cbf; border-radius: 4px; }
QSlider::handle:horizontal {
    background: #e0e0e6; border: 2px solid #7b2cbf;
    width: 16px; margin: -6px 0; border-radius: 10px;
}
QSlider::handle:horizontal:hover { background: #9d4edd; border-color: #b070ff; }
QLineEdit {
    background-color: #2b2b35; border: 1px solid #3a3a48;
    border-radius: 8px; padding: 7px 12px; color: #e0e0e6;
    selection-background-color: #7b2cbf;
}
QLineEdit:focus { border-color: #7b2cbf; }
QComboBox {
    background-color: #2b2b35; border: 1px solid #3a3a48;
    border-radius: 8px; padding: 6px 10px; color: #e0e0e6; min-height: 22px;
}
QComboBox:hover { border-color: #7b2cbf; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #23232b; color: #e0e0e6;
    border: 1px solid #3a3a48; border-radius: 8px;
    selection-background-color: #7b2cbf;
}
QListWidget {
    background-color: #23232b; border: 1px solid #3a3a48;
    border-radius: 8px; padding: 4px; color: #e0e0e6;
}
QListWidget::item { padding: 8px 12px; border-radius: 6px; }
QListWidget::item:selected { background-color: #7b2cbf; color: white; }
QListWidget::item:hover:!selected { background-color: #2f2f3a; }
QLabel[role="title"] { font-size: 20px; font-weight: 800; color: #ffffff; }
QLabel[role="status"] { color: #a0a0b0; padding: 4px; }
QLabel[role="info"] { color: #b0b0c0; padding: 6px 10px; background: #1a1a20; border-radius: 8px; }
QLabel[role="value"] { color: #b070ff; font-weight: 700; min-width: 50px; }
QLabel[role="hex"] {
    color: #b070ff; font-weight: 700; font-family: 'Consolas', monospace;
    font-size: 15px; letter-spacing: 1px;
}
QLabel[role="desc"] {
    color: #b0b0c0; padding: 6px 10px; background: #1a1a20;
    border-radius: 8px; font-style: italic;
}
QLabel[role="dot"] { font-size: 18px; color: #ef4444; }
QGroupBox {
    background-color: #23232b; border: 1px solid #2d2d38;
    border-radius: 12px; margin-top: 14px;
    padding: 14px 12px 12px 12px; font-weight: 700;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 14px; padding: 0 8px;
    color: #b070ff; background-color: #1e1e24; border-radius: 4px;
}
QFrame[role="card"] { background-color: #23232b; border: 1px solid #2d2d38; border-radius: 10px; }
QFrame#popupCard { background-color: #23232b; border: 1px solid #3a3a48; border-radius: 14px; }
QMenu { background-color: #23232b; border: 1px solid #3a3a48; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 8px 18px; border-radius: 6px; color: #e0e0e6; }
QMenu::item:selected { background-color: #7b2cbf; color: white; }
QMenu::separator { height: 1px; background: #3a3a48; margin: 4px 8px; }
QCheckBox { color: #e0e0e6; spacing: 8px; }
QCheckBox::indicator {
    width: 18px; height: 18px; border-radius: 5px;
    border: 1px solid #3a3a48; background: #2b2b35;
}
QCheckBox::indicator:checked { background: #7b2cbf; border-color: #9d4edd; }
QScrollArea { background: transparent; border: none; }
QScrollBar:vertical { background: #1e1e24; width: 10px; border-radius: 5px; }
QScrollBar::handle:vertical { background: #3a3a48; border-radius: 5px; min-height: 20px; }
QScrollBar::handle:vertical:hover { background: #7b2cbf; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
)qss");

} // namespace elkbledom::config
