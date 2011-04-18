/********************************************************************
  Copyright: 2010 Alexander Sokoloff <sokoloff.a@gmail.ru>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License.
  version 2 as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*********************************************************************/

#include "razorpanel.h"
#include "razorpanel_p.h"
#include "razorpanelplugin.h"
#include "razorpluginmanager.h"
#include "razorpanelapplication.h"
#include "razorpanellayout.h"

#include <razorqt/readsettings.h>
#include <razorqt/razorplugininfo.h>

#include <QtCore/QDebug>
#include <QtCore/QLocale>
#include <QtCore/QTranslator>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtCore/QRect>
#include <QtGui/QMenu>
#include <QtGui/QContextMenuEvent>
#include <QtCore/QFile>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QStyle>
#include <QtCore/QFileInfo>
#include <QtCore/QLibrary>
#include <QtGui/QSpacerItem>

#include <razorqt/xdgicon.h>
#include <razorqt/xfitman.h>


#define CFG_PANEL_GROUP     "panel"

#define CFG_KEY_SCREENNUM   "desktop"
#define CFG_KEY_POSITION    "position"
#define CFG_KEY_PLUGINS     "plugins"

#define CFG_FULLKEY_PLUGINS "panel/plugins"


/************************************************
 Returns the Position by the string.
 String is one of "Top", "Left", "Bottom", "Right", string is not case sensitive.
 If the string is not correct, returns defaultValue.
 ************************************************/
RazorPanel::Position strToPosition(const QString& str, RazorPanel::Position defaultValue)
{
    if (str.toUpper() == "TOP")    return RazorPanel::PositionTop;
    if (str.toUpper() == "LEFT")   return RazorPanel::PositionLeft;
    if (str.toUpper() == "RIGHT")  return RazorPanel::PositionRight;
    if (str.toUpper() == "BOTTOM") return RazorPanel::PositionBottom;
    return defaultValue;
}


/************************************************
 Return  string representation of the position
 ************************************************/
QString positionToStr(RazorPanel::Position position)
{
    switch (position)
    {
        case RazorPanel::PositionTop:    return QString("Top");
        case RazorPanel::PositionLeft:   return QString("Left");
        case RazorPanel::PositionRight:  return QString("Right");
        case RazorPanel::PositionBottom: return QString("Bottom");
    }
}


/************************************************

 ************************************************/
RazorPanelPlugin* RazorPanelPluginInfo::instance(const QString& configFile, const QString& configSection, QObject* parent)
{
    RazorPanel* panel = qobject_cast<RazorPanel*>(parent);

    if (!panel)
        return 0;

    QLibrary* lib = loadLibrary();
    if (!lib)
        return 0;

    PluginInitFunction initFunc = (PluginInitFunction) lib->resolve("init");

    if (!initFunc)
        return 0;

    RazorPanelPluginStartInfo startInfo;
    startInfo.configFile = configFile;
    startInfo.configSection = configSection;
    startInfo.panel = panel;
    startInfo.pluginInfo = this;
    return initFunc(&startInfo, panel);
}



/************************************************

 ************************************************/
RazorPanel::RazorPanel(QWidget *parent) :
  QFrame(parent),
  d_ptr(new RazorPanelPrivate(this))
{
    Q_D(RazorPanel);
    d->init();

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_X11NetWmWindowTypeDock);
    setAttribute(Qt::WA_AlwaysShowToolTips);

    setObjectName("RazorPenel");

    connect(qApp, SIGNAL(x11PropertyNotify(XEvent*)), this, SIGNAL(x11PropertyNotify(XEvent*)));
}


/************************************************

 ************************************************/
RazorPanelPrivate::RazorPanelPrivate(RazorPanel* parent):
    QObject(parent),
    q_ptr(parent),
    mScreenNum(0)
{
    // Read command line arguments ..............
    // The first argument is config file name.
    mConfigFile = "panel2";
    if (qApp->arguments().count() > 1)
    {
        mConfigFile = qApp->arguments().at(1);
        if (mConfigFile.endsWith(".conf"))
            mConfigFile.chop(5);
    }

    mSettingsReader = new ReadSettings(mConfigFile);
    mSettings = mSettingsReader->settings();

    mLayout = new RazorPanelLayout(QBoxLayout::LeftToRight, parent);
    connect(mLayout, SIGNAL(widgetMoved(QWidget*)), this, SLOT(pluginMoved(QWidget*)));

    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(screensChangeds()));
    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), this, SLOT(screensChangeds()));
    //connect(QApplication::desktop(), SIGNAL(workAreaResized(int)), this, SLOT(screensChangeds()));

}


/************************************************

 ************************************************/
void RazorPanelPrivate::screensChangeds()
{
    if (! canPlacedOn(mScreenNum, mPosition))
        mScreenNum = findAvailableScreen(mPosition);

    realign();
}


/************************************************

 ************************************************/
void RazorPanelPrivate::init()
{
    Q_Q(RazorPanel);
    // Read theme from razor.conf ...............
    ReadSettings* razorRS = new ReadSettings("razor");
    QSettings* razorSettings = razorRS->settings();
    mTheme = razorSettings->value("theme").toString();
    delete razorRS;

    // Read settings ............................
    mSettings->beginGroup(CFG_PANEL_GROUP);
    mPosition = strToPosition(mSettings->value(CFG_KEY_POSITION).toString(), RazorPanel::PositionBottom);
    mScreenNum = mSettings->value(CFG_KEY_SCREENNUM, QApplication::desktop()->primaryScreen()).toInt();
    mSettings->endGroup();

    q->setLayout(mLayout);

    loadPlugins();
    setTheme(mTheme);
}



/************************************************

 ************************************************/
RazorPanel::~RazorPanel()
{
    Q_D(RazorPanel);
    d->saveSettings();
    delete d;
}


/************************************************

 ************************************************/
RazorPanelPrivate::~RazorPanelPrivate()
{
    qDeleteAll(mPlugins);
    delete mSettingsReader;
}


/************************************************

 ************************************************/
void RazorPanelPrivate::saveSettings()
{
    foreach (RazorPanelPlugin* plugin, mPlugins)
        plugin->saveSettings();

    mSettings->beginGroup(CFG_PANEL_GROUP);
    mSettings->setValue(CFG_KEY_SCREENNUM, mScreenNum);
    mSettings->setValue(CFG_KEY_POSITION, positionToStr(mPosition));

    QStringList pluginsStr;
    for (int i=0; i<mLayout->count(); ++i)
    {
        RazorPanelPlugin* plugin = qobject_cast<RazorPanelPlugin*>(mLayout->itemAt(i)->widget());
        if (plugin)
            pluginsStr << plugin->configId();
    }

    mSettings->setValue(CFG_KEY_PLUGINS, pluginsStr);
    mSettings->endGroup();
    mSettings->sync();
}


/************************************************

 ************************************************/
void RazorPanelPrivate::loadPlugins()
{
    Q_Q(RazorPanel);

    mAvailablePlugins.load(PLUGIN_DESKTOPS_DIR, "RazorPanel/Plugin");

    QStringList sections = mSettings->value(CFG_FULLKEY_PLUGINS).toStringList();

    foreach (QString sect, sections)
    {
        qDebug() << "** Load plugin" << sect << "*************";
        QString type = mSettings->value(sect+"/type").toString();
        if (type.isEmpty())
        {
            qWarning() << QString("Section \"%1\" not found in %2.").arg(sect, mSettings->fileName());
            continue;
        }

        RazorPanelPluginInfo* pi = mAvailablePlugins.find(type);
        if (!pi)
        {
            qWarning() << QString("Plugin \"%1\" not found.").arg(type);
            continue;
        }

        RazorPanelPlugin* plugin = pi->instance(mSettings->fileName(), sect, q);
        if (!plugin)
            continue;

        mPlugins.append(plugin);
    }


    QList<RazorPanelPlugin*>::iterator i;
    // Add left plugins .........................
    for (i=mPlugins.begin(); i != mPlugins.end(); ++i)
    {
        if ((*i)->alignment() != RazorPanelPlugin::AlignLeft)
            break;
        mLayout->addWidget((*i));
    }

    mLayout->addStretch();
    mSpacer = mLayout->itemAt(mLayout->count() -1);

    // Add right plugins ........................
    for (; i != mPlugins.end(); ++i)
    {
        mLayout->addWidget((*i));
    }
}


/************************************************

 ************************************************/
void RazorPanelPrivate::setTheme(const QString& themeName)
{
    mTheme = themeName;
    ReadTheme* readTheme = new ReadTheme(themeName);
    qApp->setStyleSheet(readTheme->qss());

    delete readTheme;
    realign();
}



/************************************************

 ************************************************/
void RazorPanelPrivate::realign()
{
    Q_Q(RazorPanel);
    /*
    qDebug() << "Realign: DesktopNum" << mScreenNum;
    qDebug() << "Realign: Position  " << positionToStr(mPosition);
    qDebug() << "Realign: Theme     " << mTheme;
    qDebug() << "Realign: SizeHint  " << q->sizeHint();
    qDebug() << "Realign: Screen    " << QApplication::desktop()->screenGeometry(mScreenNum);
    */

    // Update stylesheet ............
    q->style()->unpolish(q);
    q->style()->polish(q);
    // ..............................

    if (q->isHorizontal()) mLayout->setDirection(QBoxLayout::LeftToRight);
    else  mLayout->setDirection(QBoxLayout::TopToBottom);

    QRect screen = QApplication::desktop()->screenGeometry(mScreenNum);
    QRect rect = screen;
    QSize sizeHint = q->sizeHint();
    switch (mPosition)
    {
        case RazorPanel::PositionTop:
            rect.setHeight(sizeHint.height());
            break;

        case RazorPanel::PositionBottom:
            rect.setHeight(sizeHint.height());
            rect.moveTop(screen.bottom() - sizeHint.height());
            break;

        case RazorPanel::PositionLeft:
            rect.setWidth(sizeHint.width());
            break;

        case RazorPanel::PositionRight:
            rect.setWidth(sizeHint.width());
            rect.moveLeft(screen.right() - sizeHint.width());
            break;
    }


    q->setGeometry(rect);

    // Reserve our space on the screen ..........
    XfitMan xf = xfitMan();
    Window wid = q->effectiveWinId();
    QRect desktop = QApplication::desktop()->screen(mScreenNum)->geometry();

    switch (mPosition)
    {
        case RazorPanel::PositionTop:
            xf.setStrut(wid, 0, 0, q->height(), 0,
               /* Left   */   0, 0,
               /* Right  */   0, 0,
               /* Top    */   rect.left(), rect.right(),
               /* Bottom */   0, 0
                         );
        break;

        case RazorPanel::PositionBottom:
            xf.setStrut(wid,  0, 0, 0, desktop.height() - rect.y(),
               /* Left   */   0, 0,
               /* Right  */   0, 0,
               /* Top    */   0, 0,
               /* Bottom */   rect.left(), rect.right()
                         );
            break;

        case RazorPanel::PositionLeft:
            xf.setStrut(wid, q->width(), 0, 0, 0,
               /* Left   */   rect.top(), rect.bottom(),
               /* Right  */   0, 0,
               /* Top    */   0, 0,
               /* Bottom */   0, 0
                         );

            break;

        case RazorPanel::PositionRight:
            xf.setStrut(wid, 0, desktop.width() - rect.x(), 0, 0,
               /* Left   */   0, 0,
               /* Right  */   rect.top(), rect.bottom(),
               /* Top    */   0, 0,
               /* Bottom */   0, 0
                         );
            break;
    }

}


/************************************************
  The panel can't be placed on boundary of two displays.
  This function checks, is the panel can be placed on the display
  @displayNum on @position.
 ************************************************/
bool RazorPanelPrivate::canPlacedOn(int screenNum, RazorPanel::Position position) const
{
    QDesktopWidget* dw = QApplication::desktop();

    switch (position)
    {
        case RazorPanel::PositionTop:
            for (int i=0; i < dw->screenCount(); ++i)
                if (dw->screenGeometry(i).bottom() < dw->screenGeometry(screenNum).top())
                    return false;
                return true;

        case RazorPanel::PositionBottom:
            for (int i=0; i < dw->screenCount(); ++i)
                if (dw->screenGeometry(i).top() > dw->screenGeometry(screenNum).bottom())
                    return false;
                return true;

        case RazorPanel::PositionLeft:
//            for (int i=0; i < dw->screenCount(); ++i)
//                if (dw->screenGeometry(i).right() < dw->screenGeometry(screenNum).left())
//                    return false;
//            return true;
            return false;

        case RazorPanel::PositionRight:
//            for (int i=0; i < dw->screenCount(); ++i)
//                if (dw->screenGeometry(i).left() > dw->screenGeometry(screenNum).right())
//                    return false;
//            return true;
            return false;
    }
}


/************************************************

 ************************************************/
int RazorPanelPrivate::findAvailableScreen(RazorPanel::Position position)
{
    for (int i = mScreenNum; i < QApplication::desktop()->screenCount(); ++i)
    {
        if (canPlacedOn(i, position))
            return i;
    }

    for (int i = 0; i < mScreenNum; ++i)
    {
        if (canPlacedOn(i, position))
            return i;
    }

    return 0;
}


/************************************************

 ************************************************/
PositionAction::PositionAction(int displayNum, RazorPanel::Position position, QActionGroup *parent):
    QAction(parent)
{
    if (QApplication::desktop()->screenCount() == 1)
    {
        switch (position)
        {
            case RazorPanel::PositionTop:    setText(tr("Top of desktop"));      break;
            case RazorPanel::PositionBottom: setText(tr("Bottom of desktop"));   break;
            case RazorPanel::PositionLeft:   setText(tr("Left of desktop"));     break;
            case RazorPanel::PositionRight:  setText(tr("Right of desktop"));    break;
        }
    }
    else
    {
        switch (position)
        {
        case RazorPanel::PositionTop:    setText(tr("Top of desktop %1").arg(displayNum +1));    break;
        case RazorPanel::PositionBottom: setText(tr("Bottom of desktop %1").arg(displayNum +1)); break;
        case RazorPanel::PositionLeft:   setText(tr("Left of desktop %1").arg(displayNum +1));   break;
        case RazorPanel::PositionRight:  setText(tr("Right of desktop %1").arg(displayNum +1));  break;
        }
    }


    mPosition = position;
    mDisplayNum = displayNum;
    setCheckable(true);
    parent->addAction(this);
}


/************************************************

 ************************************************/
void RazorPanelPrivate::pluginMoved(QWidget* pluginWidget)
{
    RazorPanelPlugin* plugin = qobject_cast<RazorPanelPlugin*>(pluginWidget);
    if (!plugin)
        return;

    for (int i=0; i<mLayout->count(); ++i)
    {
        if (mLayout->itemAt(i) == mSpacer)
        {
            plugin->setAlignment(RazorPanelPlugin::AlignRight);
            return;
        }

        if (mLayout->itemAt(i)->widget() == plugin)
        {
            plugin->setAlignment(RazorPanelPlugin::AlignLeft);
            return;
        }
    }
}


/************************************************

 ************************************************/
void RazorPanel::show()
{
    Q_D(RazorPanel);
    QWidget::show();
    d->realign();
    xfitMan().moveWindowToDesktop(this->effectiveWinId(), -1);
}


/************************************************

 ************************************************/
RazorPanel::Position RazorPanel::position() const
{
    Q_D(const RazorPanel);
    return d->position();
}


/************************************************

 ************************************************/
void RazorPanel::contextMenuEvent(QContextMenuEvent* event)
{
    Q_D(RazorPanel);
    d->contextMenuEvent(event);
}


/************************************************

 ************************************************/
void RazorPanelPrivate::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = new QMenu();
    menu->addMenu(popupMenu(menu));

    menu->addSeparator();
    QAction* a = menu->addAction(XdgIcon::fromTheme("application-exit", 32), "Exit");
    connect(a, SIGNAL(triggered()), qApp, SLOT(quit()));

    menu->exec(event->globalPos());
    delete menu;
}


/************************************************

 ************************************************/
QMenu* RazorPanel::popupMenu(QWidget *parent) const
{
    Q_D(const RazorPanel);
    return d->popupMenu(parent);
}


/************************************************

 ************************************************/
QMenu* RazorPanelPrivate::popupMenu(QWidget *parent) const
{
    QMenu* menu = new QMenu(tr("Show this panel at"), parent);
    menu->setIcon(XdgIcon::fromTheme("configure-toolbars", 32));
    QAction* a;

    // Create Panel menu ********************************************
     QActionGroup* posGroup = new QActionGroup(menu);

    QDesktopWidget* dw = QApplication::desktop();
    for (int i=0; i<dw->screenCount(); ++i)
    {
        if (canPlacedOn(i, RazorPanel::PositionTop))
        {
            a = new PositionAction(i,  RazorPanel::PositionTop, posGroup);
            a->setChecked(mPosition == RazorPanel::PositionTop && mScreenNum == i);
            connect(a, SIGNAL(triggered()), this, SLOT(switchPosition()));
            menu->addAction(a);
        }

        if (canPlacedOn(i, RazorPanel::PositionBottom))
        {
            a = new PositionAction(i, RazorPanel::PositionBottom, posGroup);
            a->setChecked(mPosition == RazorPanel::PositionBottom && mScreenNum == i);
            connect(a, SIGNAL(triggered()), this, SLOT(switchPosition()));
            menu->addAction(a);
        }

        if (canPlacedOn(i, RazorPanel::PositionLeft))
        {
            a = new PositionAction(i, RazorPanel::PositionLeft, posGroup);
            a->setChecked(mPosition == RazorPanel::PositionLeft && mScreenNum == i);
            connect(a, SIGNAL(triggered()), this, SLOT(switchPosition()));
            menu->addAction(a);
        }


        if (canPlacedOn(i, RazorPanel::PositionRight))
        {
            a = new PositionAction(i, RazorPanel::PositionRight, posGroup);
            a->setChecked(mPosition == RazorPanel::PositionRight && mScreenNum == i);
            connect(a, SIGNAL(triggered()), this, SLOT(switchPosition()));
            menu->addAction(a);
        }

        menu->addSeparator();
    }

    return menu;
}


/************************************************

 ************************************************/
void RazorPanelPrivate::switchPosition()
{
    PositionAction* a = qobject_cast<PositionAction*>(sender());
    if (!a)
        return;

    Q_Q(RazorPanel);
    mPosition = a->position();
    mScreenNum = a->displayNum();
    realign();
    emit q->positionChanged();
}

