// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fieldspec.h"

#include <QList>
#include <QString>

#include <functional>
#include <utility>

/// One `--preset FIELD:CHOICE:NAME=VALUE[:NAME=VALUE...]` argument: picking CHOICE in the combo
/// FIELD sets the named fields to these values.
struct PresetSpec
{
    QString field;
    QString choice;
    QList<std::pair<QString, QString>> values;

    /// Returns a spec with an empty `field` when `spec` does not parse; `error` then carries the
    /// message meant for stderr.
    static PresetSpec parse(const QString& spec, QString* error);
};

/// Checks every preset against the fields it names and rewrites its values into the spelling the
/// form prints - `on` for a ticked `true`, `28` for `028` - so a preset can be compared with what
/// the form holds. Returns false with `error` set on the first preset that does not fit.
bool resolvePresets(QList<PresetSpec>* presets, const QList<FieldSpec>& fields, QString* error);

/// The choice of `combo` that the form's current values amount to: `current` if it has no preset or
/// its preset still holds, otherwise the first choice whose preset does, otherwise the first choice
/// without a preset. Empty when none of those exists, which means "leave the combo as it is".
QString matchingChoice(const FieldSpec& combo, const QString& current, const QList<PresetSpec>& presets,
                       const std::function<QString(const QString&)>& valueOf);
