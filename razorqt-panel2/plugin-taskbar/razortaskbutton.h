/********************************************************************
  Copyright: 2011 Alexander Sokoloff <sokoloff.a@gmail.ru>

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

#ifndef RAZORTASKBUTTON_H
#define RAZORTASKBUTTON_H

#include <QtGui/QToolButton>
#include <QtCore/QHash>


#include <X11/X.h>
#include <X11/Xlib.h>


class RazorTaskButton : public QToolButton
{
    Q_OBJECT
public:
    explicit RazorTaskButton(const Window window, QWidget *parent = 0);
    virtual ~RazorTaskButton();

    bool isAppHidden() const;
    bool isApplicationActive() const;
    Window windowId() const { return mWindow; }

    QSize   sizeHint() const;
    static void unCheckAll();

public slots:
    void raiseApplication();
    void minimizeApplication();
    void maximizeApplication();
    void shadeApplication();
    void unShadeApplication();
    void closeApplication();
    void moveApplicationToDesktop();

    void handlePropertyNotify(XPropertyEvent* event);

protected:
    void nextCheckState();
    void contextMenuEvent( QContextMenuEvent* event);

    void updateText();
    void updateIcon();

private:
    Window mWindow;
    static RazorTaskButton* mCheckedBtn;

private slots:
    void btnClicked(bool checked);
    void checkedChanged(bool checked);
};

typedef QHash<Window,RazorTaskButton*> RazorTaskButtonHash;

#endif // RAZORTASKBUTTON_H
