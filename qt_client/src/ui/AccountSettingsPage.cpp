#include "AccountSettingsPage.h"

#include "Avatar.h"
#include "AvatarCropDialog.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QPushButton *makeButton(const QString &text, const char *objectName) {
    auto *button = new QPushButton(text);
    button->setObjectName(objectName);
    button->setFixedSize(64, 32);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

AccountSettingsPage::AccountSettingsPage(const QString &username, int uid, const QString &email,
                                         QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *heading = new QLabel(tr("账号信息"));
    heading->setObjectName("settingsHeading");
    // 头像放在最上面：左边头像，右边“上传新头像”
    layout->addWidget(makeAvatarRow());
    layout->addSpacing(8);
    layout->addWidget(heading);
    layout->addSpacing(4);

    layout->addWidget(makeRow(tr("用户名"), m_name, true));
    layout->addWidget(makeRow(tr("账号 UID"), m_uid, false));
    layout->addWidget(makeRow(tr("邮箱"), m_email, true));
    m_uid.value->setText(QString::number(uid));
    m_email.edit->setPlaceholderText(tr("name@example.com"));

    // 保存结果的提示（成功 / 失败共用一个标签，颜色由 QSS 按 error 属性区分）
    m_message = new QLabel;
    m_message->setObjectName("settingsMessage");
    m_message->setWordWrap(true);
    layout->addWidget(m_message);
    layout->addStretch();

    setProfile(username, email);
    m_message->clear();   // setProfile 会写“已保存”，刚打开时不需要

    connect(m_name.saveButton, &QPushButton::clicked, this, &AccountSettingsPage::submitName);
    connect(m_name.edit, &QLineEdit::returnPressed, this, &AccountSettingsPage::submitName);
    connect(m_email.saveButton, &QPushButton::clicked, this, &AccountSettingsPage::submitEmail);
    connect(m_email.edit, &QLineEdit::returnPressed, this, &AccountSettingsPage::submitEmail);
}

QWidget *AccountSettingsPage::makeRow(const QString &name, Row &row, bool editable) {
    auto *widget = new QWidget;
    widget->setObjectName("settingsRow");
    widget->setAttribute(Qt::WA_StyledBackground);
    widget->setFixedHeight(56);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 0, 12, 0);
    layout->setSpacing(8);

    auto *nameLabel = new QLabel(name);
    nameLabel->setObjectName("settingsRowName");
    nameLabel->setFixedWidth(92);
    layout->addWidget(nameLabel);

    row.value = new QLabel;
    row.value->setObjectName("settingsRowValue");
    row.value->setTextInteractionFlags(Qt::TextSelectableByMouse);   // 方便复制 UID / 邮箱
    layout->addWidget(row.value, 1);
    if (!editable) return widget;

    row.edit = new QLineEdit;
    row.edit->setObjectName("settingsInput");
    row.edit->setFixedHeight(32);
    layout->addWidget(row.edit, 1);

    row.editButton = makeButton(tr("编辑"), "settingsRowButton");
    row.saveButton = makeButton(tr("保存"), "primaryButton");
    row.cancelButton = makeButton(tr("取消"), "settingsRowButton");
    layout->addWidget(row.editButton);
    layout->addWidget(row.saveButton);
    layout->addWidget(row.cancelButton);
    setEditing(row, false);

    // 这里的 row 是成员变量（m_name / m_email），生命周期和页面一样长，所以可以按引用捕获
    connect(row.editButton, &QPushButton::clicked, this, [this, &row] {
        row.edit->setText(&row == &m_email ? m_emailValue : row.value->text());
        m_message->clear();
        setEditing(row, true);
        row.edit->setFocus();
        row.edit->selectAll();
    });
    connect(row.cancelButton, &QPushButton::clicked, this, [this, &row] {
        m_message->clear();
        setEditing(row, false);
    });
    return widget;
}

QWidget *AccountSettingsPage::makeAvatarRow() {
    auto *widget = new QWidget;
    widget->setObjectName("settingsRow");
    widget->setAttribute(Qt::WA_StyledBackground);
    widget->setFixedHeight(88);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 0, 12, 0);
    layout->setSpacing(8);

    // 和其他行同一套列宽：“头像”标签占 92，后面才是头像预览
    auto *nameLabel = new QLabel(tr("头像"));
    nameLabel->setObjectName("settingsRowName");
    nameLabel->setFixedWidth(92);
    layout->addWidget(nameLabel);

    // 圆形头像预览（和通讯录 / 聊天里的头像同一套画法）
    m_avatar = new QLabel;
    m_avatar->setFixedSize(56, 56);
    m_avatar->setPixmap(avatar::user(QString(), 56));
    layout->addWidget(m_avatar);
    layout->addStretch();

    // 右边：上传按钮
    m_avatarButton = makeButton(tr("上传新头像"), "settingsRowButton");
    m_avatarButton->setFixedSize(96, 32);
    layout->addWidget(m_avatarButton);

    connect(m_avatarButton, &QPushButton::clicked, this, &AccountSettingsPage::chooseAvatar);
    return widget;
}

// 选图 → 裁剪（固定 1:1、可拖动缩放）→ 把裁好的 PNG 交给上层上传
void AccountSettingsPage::chooseAvatar() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("选择头像"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty()) return;

    AvatarCropDialog crop(path, this);
    if (!crop.isValid()) {
        showAvatarError(tr("这张图片打不开，请换一张"));
        return;
    }
    if (crop.exec() != QDialog::Accepted) return;

    const QByteArray png = crop.croppedPng();
    if (png.isEmpty()) {
        showAvatarError(tr("裁剪失败，请重试"));
        return;
    }
    m_message->clear();
    setAvatarUploading(true);
    emit avatarUploadRequested(png);
}

void AccountSettingsPage::setAvatar(const QString &path) {
    m_avatar->setPixmap(avatar::user(path, 56));
}

void AccountSettingsPage::setAvatarUploading(bool busy) {
    m_avatarButton->setEnabled(!busy);
    m_avatarButton->setText(busy ? tr("上传中…") : tr("上传新头像"));
}

void AccountSettingsPage::showAvatarDone() {
    setAvatarUploading(false);
    showMessage(tr("头像已更新"), false);
}

void AccountSettingsPage::showAvatarError(const QString &message) {
    setAvatarUploading(false);
    showMessage(message, true);
}

void AccountSettingsPage::setEditing(Row &row, bool editing) {
    row.value->setVisible(!editing);
    row.editButton->setVisible(!editing);
    row.edit->setVisible(editing);
    row.saveButton->setVisible(editing);
    row.cancelButton->setVisible(editing);
    row.saveButton->setEnabled(true);
}

void AccountSettingsPage::setProfile(const QString &username, const QString &email) {
    m_emailValue = email;
    m_name.value->setText(username);
    m_email.value->setText(email.isEmpty() ? tr("未绑定") : email);
    setEditing(m_name, false);
    setEditing(m_email, false);
    showMessage(tr("已保存"), false);
}

void AccountSettingsPage::showError(const QString &message) {
    // 编辑框留着让用户改了再试，只把“保存”恢复成可点
    m_name.saveButton->setEnabled(true);
    m_email.saveButton->setEnabled(true);
    showMessage(message, true);
}

void AccountSettingsPage::showMessage(const QString &text, bool isError) {
    m_message->setProperty("error", isError);
    // QSS 按 error 属性选颜色，但属性变了 Qt 不会自动重算样式，要手动重新套一遍
    m_message->style()->unpolish(m_message);
    m_message->style()->polish(m_message);
    m_message->setText(text);
}

void AccountSettingsPage::submitName() {
    const QString name = m_name.edit->text().trimmed();
    if (name.isEmpty()) return showError(tr("用户名不能为空"));
    if (name == m_name.value->text()) return setEditing(m_name, false);   // 没改，直接收起

    m_message->clear();
    m_name.saveButton->setEnabled(false);   // 等结果期间不让重复点
    emit nameChangeRequested(name);
}

void AccountSettingsPage::submitEmail() {
    const QString email = m_email.edit->text().trimmed();
    if (email.isEmpty() || !email.contains(QLatin1Char('@'))) return showError(tr("请输入有效的邮箱地址"));
    if (email == m_emailValue) return setEditing(m_email, false);

    m_message->clear();
    m_email.saveButton->setEnabled(false);
    emit emailChangeRequested(email);
}
