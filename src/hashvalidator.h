/*
 * Copyright 2017 Alexander Fasching
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QValidator>
#include <QByteArray>


class HashValidator : public QValidator
{
    Q_OBJECT

public:
    explicit HashValidator(QObject *parent = 0) :
        QValidator(parent)
    {

    }

    virtual State validate(QString &input, int &pos) const
    {
        auto low = input.toLower();
        for(int i=0; i<low.size(); i++) {
            if(low[i] < '0' or low[i] > 'f') {
                emit validityChanged(false);
                return Invalid;
            }
        }

        if(low.size() < 40) {
            emit validityChanged(false);
            return Intermediate;
        }

        if(low.size() > 40) {
            return Invalid;
        }

        emit validityChanged(true);

        return Acceptable;
    }

signals:
    void validityChanged(bool valid) const;
};
