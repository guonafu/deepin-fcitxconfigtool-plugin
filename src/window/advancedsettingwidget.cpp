/*
* Copyright (C) 2021 ~ 2021 Deepin Technology Co., Ltd.
*
* Author:     zhaoyue <zhaoyue@uniontech.com>
*
* Maintainer: zhaoyue <zhaoyue@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "advancedsettingwidget.h"
// Qt
#include <QTabWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QStackedLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QPushButton>
#include <QDebug>
#include <QStandardItemModel>
#include <QListView>
#include <QComboBox>
#include <QLineEdit>
#include <QMatrix>
#include <DButtonBox>
#include <DPushButton>
#include <DSwitchButton>
#include <KLocalizedString>
#include <DSpinBox>
// system
#include <libintl.h>

// Fcitx
#include <fcitx-config/fcitx-config.h>
#include <fcitx-config/hotkey.h>
#include <fcitx-config/xdg.h>
#include <fcitxqtkeysequencewidget.h>

// self
#include "fcitxInterface/config.h"
#include "fcitxInterface/dummyconfig.h"
#include "fcitxInterface/verticalscrollarea.h"
#include "widgets/keysettingsitem.h"
#include "window/immodel/imconfig.h"
#include "fcitxInterface/global.h"

#define RoundColor(c) ((c)>=0?((c)<=255?c:255):0)
DWIDGET_USE_NAMESPACE
namespace Fcitx
{

static
void SyncFilterFunc(FcitxGenericConfig* gconfig, FcitxConfigGroup *group, FcitxConfigOption *option, void *value, FcitxConfigSync sync, void *arg);


AdvancedSettingWidget::AdvancedSettingWidget(QWidget* parent)
    : QWidget(parent)
    , m_hash(new QHash<QString, FcitxConfigFileDesc *>)
    , m_prefix("")
    , m_name("config")
    , m_addonName("global")
    , m_globalSettingsLayout(new QVBoxLayout)
    , m_addOnsLayout(new QVBoxLayout)
    , m_simpleUiType(CW_NoShow)
    , m_fullUiType(CW_NoShow)
{
    getConfigDesc("config.desc");
    setupConfigUi();
}

void AdvancedSettingWidget::setupConfigUi()
{
    DButtonBoxButton *btnGlobalSettings = new DButtonBoxButton(tr("Global Settings"));
    btnGlobalSettings->setAccessibleName("globalSettings");
    DButtonBoxButton *btnAddOns = new DButtonBoxButton(tr("Add ons"));
    btnAddOns->setAccessibleName("addons");
    QList<DButtonBoxButton *> pBtnlist;
    pBtnlist.append(btnGlobalSettings);
    pBtnlist.append(btnAddOns);
    DButtonBox *pTopSwitchWidgetBtn = new DButtonBox;
    pTopSwitchWidgetBtn->setButtonList(pBtnlist, true);
    pBtnlist.first()->setChecked(true);
    pTopSwitchWidgetBtn->setId(btnGlobalSettings, 0);
    pTopSwitchWidgetBtn->setId(btnAddOns, 1);
    pTopSwitchWidgetBtn->setMinimumSize(240, 36);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(pTopSwitchWidgetBtn,  0, Qt::AlignHCenter);
    QStackedLayout *stackLayout = new QStackedLayout;
    QWidget *globalSettingsWidget = new QWidget;
    globalSettingsWidget->setLayout(m_globalSettingsLayout);
    QWidget *addOnsWidget = new QWidget;
    addOnsWidget->setLayout(m_addOnsLayout);
    stackLayout->addWidget(globalSettingsWidget);
    stackLayout->addWidget(addOnsWidget);
    layout->addLayout(stackLayout);
    connect(btnGlobalSettings, &DButtonBoxButton::clicked, this, [=](){
        stackLayout->setCurrentIndex(0);
    });
    connect(btnAddOns, &DButtonBoxButton::clicked, this, [=](){
        stackLayout->setCurrentIndex(1);
    });
    setLayout(layout);

    if (m_cfdesc) {
        bindtextdomain(m_cfdesc->domain, LOCALEDIR);
        bind_textdomain_codeset(m_cfdesc->domain, "UTF-8");
        FILE *fp;
        fp = FcitxXDGGetFileWithPrefix(m_prefix.toLocal8Bit().constData(), m_name.toLocal8Bit().constData(), "r", nullptr);
        m_config->load(fp);

        if (fp)
            fclose(fp);
    }
    m_widget = createConfigUi();
    m_globalSettingsLayout->addWidget(m_widget);

    if (m_config)
        m_config->sync();
}

AdvancedSettingWidget::~AdvancedSettingWidget()
{
    if (m_config)
        delete m_config;
}

void AdvancedSettingWidget::createConfigOptionWidget(FcitxConfigGroupDesc* cgdesc, FcitxConfigOptionDesc* codesc, QString& label,
                                                     QString& tooltip, QWidget*& inputWidget, void*& newarg)
{
    FcitxConfigOptionDesc2* codesc2 = (FcitxConfigOptionDesc2*) codesc;

    void* oldarg = nullptr;
    void* argument = nullptr;
    QString strrrr = codesc->shownInDeepin;
    QString name(QString("%1/%2").arg(cgdesc->groupName).arg(codesc->optionName));
    if (m_argsMap.contains(name)) {
        oldarg = m_argsMap[name];
    }

    if (codesc->desc && codesc->desc[0])
        label = QString::fromUtf8(dgettext(m_cfdesc->domain, codesc->desc));
    else
        label = QString::fromUtf8(dgettext(m_cfdesc->domain, codesc->optionName));

    if (codesc2->longDesc && codesc2->longDesc[0]) {
        tooltip = QString::fromUtf8(dgettext(m_cfdesc->domain, codesc2->longDesc));
    }

    switch (codesc->type) {

    case T_Integer: {
        DSpinBox* spinbox = new DSpinBox(this);
        spinbox->setEnabledEmbedStyle(true);
        spinbox->setMinimumWidth(50);
        spinbox->setMaximum(codesc2->constrain.integerConstrain.max);
        spinbox->setMinimum(codesc2->constrain.integerConstrain.min);
        inputWidget = spinbox;
        connect(spinbox,  QOverload<int>::of(&DSpinBox::valueChanged), this, [=](int value) {
            IMConfig::setIMvalue(codesc->optionName, QString().number(value));
            Global::instance()->inputMethodProxy()->ReloadConfig();
        });
        break;
    }

    case T_Boolean: {
        DSwitchButton *pSwitchBtn = new DSwitchButton;
        if(QString(codesc->rawDefaultValue).contains("True")) {
           pSwitchBtn->setChecked(true);
        }
        inputWidget = pSwitchBtn;
        connect(pSwitchBtn, &DSwitchButton::clicked, this, [=](bool checked) {
            IMConfig::setIMvalue(codesc->optionName, checked ? "True" : "False");
            Global::instance()->inputMethodProxy()->ReloadConfig();
        });
        break;
    }

    case T_Enum: {
        int i;
        FcitxConfigEnum *e = &codesc->configEnum;
        QComboBox* combobox = new QComboBox(this);
        combobox->setMinimumWidth(150);
        inputWidget = combobox;

        for (i = 0; i < e->enumCount; i ++) {
            combobox->addItem(QString::fromUtf8(dgettext(m_cfdesc->domain, e->enumDesc[i])));
            combobox->setItemData(i, e->enumDesc[i]);
        }
        connect(combobox, &QComboBox::currentTextChanged, this, [=](const QString& text) {
            int index = combobox->findText(text);
            IMConfig::setIMvalue(codesc->optionName, combobox->itemData(index).toString());
            Global::instance()->inputMethodProxy()->ReloadConfig();
        });
    }

    break;

    case T_Hotkey: {
        FcitxKeySettingsItem* item = new FcitxKeySettingsItem;
        item->setList(QString(codesc->rawDefaultValue).split(' ').first().split('_'));
        if(QString(codesc->rawDefaultValue).isEmpty()) {
            item->setList(QString(tr("None")).split('_'));
        }
        inputWidget = item;
        connect(item, &FcitxKeySettingsItem::editedFinish, [ = ]() {
            QString str = item->getKeyToStr();
            IMConfig::setIMvalue(codesc->optionName, item->getKeyToStr());
            item->setList(item->getKeyToStr().split("_"));
            Global::instance()->inputMethodProxy()->ReloadConfig();
        });
    }

    break;

    case T_Char:

    case T_String: {
        QLineEdit* lineEdit = new QLineEdit(this);
        lineEdit->setMinimumWidth(150);
        inputWidget = lineEdit;
        argument = inputWidget;
        connect(lineEdit, &QLineEdit::editingFinished, [ = ]() {
            IMConfig::setIMvalue(codesc->optionName, lineEdit->text());
            Global::instance()->inputMethodProxy()->ReloadConfig();
        });
    }

    break;

    case T_I18NString:
        inputWidget = nullptr;
        argument = nullptr;
        break;
    default:
        break;
    }


    if (inputWidget && !tooltip.isEmpty())
        inputWidget->setToolTip(tooltip);

    if (inputWidget) {
        m_argsMap[name] = inputWidget;
        newarg = inputWidget;
    }
}

QWidget* AdvancedSettingWidget::createConfigUi()
{
    int row = 0;
    VerticalScrollArea *scrollarea = new VerticalScrollArea;
    scrollarea->setFrameStyle(QFrame::NoFrame);
    scrollarea->setWidgetResizable(true);

    QWidget* form = new QWidget;
    QVBoxLayout* vgLayout = new QVBoxLayout;
    scrollarea->setWidget(form);
    form->setLayout(vgLayout);

    do {
        if (!m_cfdesc)
            break;

        if (!m_config->isValid())
            break;

        HASH_FOREACH(cgdesc, m_cfdesc->groupsDesc, FcitxConfigGroupDesc) {
            QLabel* grouplabel = new QLabel(QString("<b>%1</b>").arg(QString::fromUtf8(dgettext(m_cfdesc->domain, cgdesc->groupName))));
            QFont f;
            f.setPixelSize(17);
            grouplabel->setFont(f);
            arrowButton *button =new arrowButton();
            QPixmap pmap = QIcon::fromTheme("dm_arrow").pixmap(QSize(12, 8));
            QMatrix matrix;
            matrix.rotate(180);
            button->setPixmap(pmap.transformed(matrix, Qt::SmoothTransformation));
            button->setMaximumWidth(50);
            QHBoxLayout *hglayout = new QHBoxLayout;
            hglayout->addWidget(grouplabel);
            hglayout->addWidget(button, Qt::AlignRight);
            vgLayout->addLayout(hglayout);
            QWidget *content = new QWidget;
            QVBoxLayout *vlayout = new QVBoxLayout;
            content->setLayout(vlayout);
            content->setHidden(true);
            connect(button, &arrowButton::pressed, this, [=](bool isHidden) {
                content->setHidden(isHidden);
                QMatrix matrix;
                if(isHidden) {
                    matrix.rotate(180);
                } else {
                    matrix.rotate(0);
                }

                button->setPixmap(pmap.transformed(matrix, Qt::SmoothTransformation));
            });
            vgLayout->addWidget(content, Qt::AlignLeft);
            HASH_FOREACH(codesc, cgdesc->optionsDesc, FcitxConfigOptionDesc) {
                QString s, tooltip;
                QWidget* inputWidget = nullptr;
                void* argument = nullptr;
                QString shownInDeepin = codesc->shownInDeepin;
                if(shownInDeepin.contains("True")) {
                    createConfigOptionWidget(cgdesc, codesc, s, tooltip, inputWidget, argument);
                }
                if (inputWidget) {
                    QLabel* label = new QLabel(s);
                    QFont f;
                    f.setPixelSize(13);
                    label->setFont(f);
                    label->setMinimumWidth(100);
                    if (!tooltip.isEmpty()) {
                        label->setToolTip(tooltip);
                    }
                    QHBoxLayout *hlayout = new QHBoxLayout;
                    hlayout->addWidget(label);
                    hlayout->addWidget(inputWidget);
                    vlayout->addLayout(hlayout);
                    //gridLayout->addWidget(label, row, 0, Qt::AlignLeft);
                    //gridLayout->addWidget(inputWidget, row, 1, Qt::AlignRight);
                    if (argument)
                        m_config->bind(cgdesc->groupName, codesc->optionName, SyncFilterFunc, argument);
                    row++;
                }
            }
        }
    } while(0);


    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

    if (row >= 2) {
        QSpacerItem* horizontalSpacer = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
        vgLayout->addItem(horizontalSpacer);
    }

    vgLayout->addItem(verticalSpacer);

    return scrollarea;
}

void AdvancedSettingWidget::getConfigDesc(const QString &name)
{
    if (m_hash->count(name) <= 0) {
        FILE *fp = FcitxXDGGetFileWithPrefix("configdesc", name.toLatin1().constData(), "r", NULL);
        m_cfdesc = FcitxConfigParseConfigFileDescFp(fp);

        if (m_cfdesc)
            m_hash->insert(name, m_cfdesc);


    } else {
        m_cfdesc = (*m_hash)[name];
    }
    if (m_cfdesc) {
        m_config = new DummyConfig(m_cfdesc);
    }
    return ;
}

void SyncFilterFunc(FcitxGenericConfig* gconfig, FcitxConfigGroup *group, FcitxConfigOption *option, void *value, FcitxConfigSync sync, void *arg)
{
    Q_UNUSED(gconfig);
    Q_UNUSED(group);
    Q_UNUSED(value);
    FcitxConfigOptionDesc *codesc = option->optionDesc;

    if (!codesc)
        return;

    if (sync == Raw2Value) {
        switch (codesc->type) {

        case T_I18NString:
            break;

        case T_Integer: {
            int i = *(int*) value;
            DSpinBox* spinbox = static_cast<DSpinBox*>(arg);
            spinbox->setValue(i);
        }

        break;

        case T_Boolean: {
            boolean *bl = static_cast<boolean*>(value);

            DSwitchButton* checkBox = static_cast<DSwitchButton*>(arg);

            checkBox->setChecked(*bl);
        }

        break;

        case T_Enum: {
            int index = *(int*) value;

            QComboBox* combobox = static_cast<QComboBox*>(arg);

            combobox->setCurrentIndex(index);
        }

        break;

        case T_Hotkey: {
            FcitxHotkey* hotkey = static_cast<FcitxHotkey*>(value);
            FcitxKeySettingsItem* item = static_cast<FcitxKeySettingsItem*>(arg);
            item->setList(QString(hotkey->desc).split("_"));
        }

        break;

        case T_Char: {
            char* string = static_cast<char*>(value);
            char temp[2] = { *string, '\0' };
            QLineEdit* lineEdit = static_cast<QLineEdit*>(arg);
            lineEdit->setText(QString::fromUtf8(temp));
        }
        break;

        case T_String: {
            char** string = (char**) value;
            QLineEdit* lineEdit = static_cast<QLineEdit*>(arg);
            lineEdit->setText(QString::fromUtf8(*string));
        }

        break;
        }
    } else {
        if (codesc->type != T_I18NString && option->rawValue) {
            free(option->rawValue);
            option->rawValue = nullptr;
        }

        switch (codesc->type) {

        case T_I18NString:
            break;

        case T_Integer: {
            int* i = (int*) value;
            DSpinBox* spinbox = static_cast<DSpinBox*>(arg);
            *i = spinbox->value();
        }

        break;

        case T_Boolean: {
            QCheckBox* checkBox = static_cast<QCheckBox*>(arg);
            boolean *bl = static_cast<boolean*>(value);
            *bl = checkBox->isChecked();
        }

        break;

        case T_Enum: {
            QComboBox* combobox = static_cast<QComboBox*>(arg);
            int* index = static_cast<int*>(value);
            *index = combobox->currentIndex();
        }

        break;

        case T_Hotkey: {
            FcitxKeySettingsItem* item = static_cast<FcitxKeySettingsItem*>(arg);
            FcitxHotkey* hotkey = static_cast<FcitxHotkey*>(value);
            item->setList(QString(hotkey->desc).split("_"));
        }

        break;

        case T_Char: {
            QLineEdit* lineEdit = static_cast<QLineEdit*>(arg);
            char* c = static_cast<char*>(value);
            *c = lineEdit->text()[0].toLatin1();
        }
        break;

        case T_String: {
            QLineEdit* lineEdit = static_cast<QLineEdit*>(arg);
            char** string = (char**) value;
            fcitx_utils_string_swap(string, lineEdit->text().toUtf8().constData());
        }
        break;
        }
    }
}

arrowButton::arrowButton(QWidget *parent)
    : QLabel(parent)
{

}

void arrowButton::mousePressEvent(QMouseEvent *ev)
{
    emit pressed(m_hide);
    m_hide = !m_hide;
}

}