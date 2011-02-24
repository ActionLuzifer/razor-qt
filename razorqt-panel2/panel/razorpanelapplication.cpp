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

#include "razorpanelapplication.h"
#include <QtCore/QDebug>
#include "razorpluginmanager.h"
#include <X11/Xlib.h>

/************************************************

 ************************************************/
RazorPanelApplication::RazorPanelApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
}


/************************************************

 ************************************************/
bool RazorPanelApplication::x11EventFilter(XEvent * event)
{
    if(event->type != PropertyNotify)
        return false;

    emit x11PropertyNotify(event);

    return false;
}


