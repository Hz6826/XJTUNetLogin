//
// Created by Hz6826 on 2026/10/15.
//

#ifndef AUTOSTART_H
#define AUTOSTART_H

class QString;

namespace AutoStart {

    bool isEnabled();
    bool setEnabled(bool enabled);

    QString identifier();
} // namespace AutoStart

#endif // AUTOSTART_H

