/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DOCUMENTATION_H
#define DOCUMENTATION_H

#include <QtCore/QString>

QT_FORWARD_DECLARE_CLASS(QDebug)

class Documentation
{
public:
    enum Format {
        Native, // XML
        Target  // RST
    };

    enum Type {
        Detailed,
        Brief,
        Last
    };

    Documentation() = default;
    explicit Documentation(const QString &detailed,
                           const QString &brief,
                           Format fmt = Documentation::Native);

    bool isEmpty() const;

    void setValue(const QString& value, Type t = Documentation::Detailed);

    Documentation::Format format() const;
    void setFormat(Format f);

    bool equals(const Documentation &rhs) const;

    const QString &detailed() const { return m_detailed; }
    void setDetailed(const QString &detailed);

    bool hasBrief() const { return !m_brief.isEmpty(); }
    const QString &brief() const { return m_brief; }
    void setBrief(const QString &brief);

private:
    QString m_detailed;
    QString m_brief;
    Format m_format = Documentation::Native;
};

inline bool operator==(const Documentation &d1, const Documentation &d2)
{ return d1.equals(d2); }
inline bool operator!=(const Documentation &d1, const Documentation &d2)
{ return !d1.equals(d2); }

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug debug, const Documentation &);
#endif

#endif // DOCUMENTATION_H
