/*
 configuration_dialog.h     MindForger thinking notebook

 Copyright (C) 2016-2018 Martin Dvorak <martin.dvorak@mindforger.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef M8RUI_CONFIGURATION_DIALOG_H
#define M8RUI_CONFIGURATION_DIALOG_H

#include <QtWidgets>

#include "../../lib/src/gear/lang_utils.h"

namespace m8r {

class ConfigurationDialog : public QObject
{
    Q_OBJECT

public:
    explicit ConfigurationDialog(QWidget* parent);
    ConfigurationDialog(const ConfigurationDialog&) = delete;
    ConfigurationDialog(const ConfigurationDialog&&) = delete;
    ConfigurationDialog &operator=(const ConfigurationDialog&) = delete;
    ConfigurationDialog &operator=(const ConfigurationDialog&&) = delete;
    ~ConfigurationDialog();
};

}
#endif // M8RUI_CONFIGURATION_DIALOG_H
