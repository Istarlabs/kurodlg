// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

enum class FieldType
{
    Invalid,
    Text,
    Password,
    Int,
    Check,
    Combo,
    File,
    Dir,
    Date,
};

/// One `--field NAME:TYPE:Label[:Default[:Extra]]` argument, parsed.
struct FieldSpec
{
    QString name;
    FieldType type = FieldType::Invalid;
    QString label;
    QString defaultValue;

    // combobox
    QStringList choices;

    // spinbox (integer)
    int minimum = 0;
    int maximum = 100;
    int number = 0;

    // checkbox
    bool checked = false;

    /// Returns a spec whose type is Invalid when `spec` does not parse; `error` then carries the
    /// message meant for stderr.
    static FieldSpec parse(const QString& spec, QString* error);
};

FieldType fieldTypeFromName(const QString& name);
