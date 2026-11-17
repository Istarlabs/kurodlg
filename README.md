<!--
SPDX-FileCopyrightText: 2026 Pascal Klose
SPDX-License-Identifier: GPL-3.0-or-later
-->

# kurodlg

Dialogs for shell scripts and Dolphin service menus, in Qt Quick with Kirigami so they look like the
rest of a Plasma session. Three things `kdialog` cannot do:

- a **progress window** with a settable size, an in-dialog heading and a collapsible details area
  that takes a command's output, the way Dolphin's own long-running operations look
- a **form** with any number of typed fields instead of one value per dialog
- a **text window** that shows what a command prints while it runs
- the progress as a **Plasma job notification** without any window (`--progress --job`)

And `--passivepopup`, as kdialog has it, so a script needs only one of the two for notifications.

This is not a kdialog fork. For message boxes, yes/no questions, file pickers and lists, keep
calling `kdialog` - it does those well and the distribution maintains it.

## Build

Needs Qt 6.7+ (with Qt Quick Controls), KF6 6.8+ (CoreAddons, Crash, I18n, IconThemes, JobWidgets,
Notifications, WindowSystem, Kirigami),
kirigami-addons, ECM and a C++26 compiler. On Fedora everything but the compiler comes from
`qt6-qtbase-devel`, `qt6-qtdeclarative-devel`, the matching `kf6-*-devel` packages,
`kf6-kirigami-addons-devel` and `extra-cmake-modules`. At run time the windows also load
`qqc2-desktop-style` (the Breeze look of Qt Quick Controls); `QT_QUICK_CONTROLS_STYLE` overrides it.

The scripts in `Scripts/` come from [devscripts](https://github.com/Istarlabs/devscripts), pinned as a submodule: clone
with `git clone --recurse-submodules`, or run `git submodule update --init` in an existing clone.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build -j
ctest --test-dir build
cmake --install build
```

## Modes

### `--form` - several fields in one dialog

```bash
kurodlg --form "Encoding options" --title "AV1" \
  --field preset:combo:Preset:p4\|*p5\|p6 \
  --field cq:int:CQ:28:10-51 \
  --field tenbit:check:10-Bit:on \
  --field out:dir:Target:"$HOME/Videos"
```

Prints one `NAME=value` line per field, in the order the fields were given, and exits 0. A
cancelled dialog prints nothing and exits 1; a field that does not parse exits 2 with the reason on
stderr.

```
preset=p5
cq=28
tenbit=on
out=/home/alex/Videos
```

A field is `NAME:TYPE:Label[:Default[:Extra]]`.

| Type | Default means | Extra | Prints |
| --- | --- | --- | --- |
| `text` | initial text | - | the text |
| `pass` | initial text, shown as dots | - | the text |
| `int` | initial number, clamped into the range | `MIN-MAX` | the number |
| `check` | `on`, `true` or `1` ticks it | - | `on` / `off` |
| `combo` | the choices, `\|`-separated, `*` marks the preselected one | - | the chosen entry |
| `file` | initial path, with a browse button in the field | - | the path |
| `dir` | initial path, with a browse button in the field | - | the path |
| `date` | ISO date, or empty for today | - | ISO date |

`NAME` is what the value is printed under, so reading the output back does not depend on the field
order. The label may not contain a colon; defaults and ranges may.

#### `--preset` - a combo entry that fills in other fields

```bash
kurodlg --form "Convert to AV1" \
  --field preset:combo:Preset:DVD\ film\|Archive\|Custom \
  --field cq:int:Quality:28:1-51 \
  --field tenbit:check:10 bit:off \
  --preset "preset:DVD film:cq=28:tenbit=off" \
  --preset "preset:Archive:cq=20:tenbit=on"
```

`FIELD:CHOICE:NAME=VALUE[:NAME=VALUE...]`, repeatable. Picking CHOICE in the combo FIELD sets the
named fields, which stay editable. Editing one of them re-selects the choice whose preset the form
holds now, or else the first choice without a preset - so the combo never names a preset the fields
have left. A choice without a preset that the user picked stays picked. The same matching runs when
the dialog opens: the field defaults win, and the combo opens on what they amount to.

The output does not change - the combo prints its choice like any other. A preset that names a
missing field, a choice the combo does not have, or a value its field cannot take (`cq=99` on a
`1-51` field, `tenbit=yes`) exits 2 with the reason. Values may not contain a colon.

### `--textbox` - a command's output in a window

```bash
ffmpeg -i in.mkv -c:v av1_nvenc out.mkv 2>&1 | kurodlg --textbox "Encoding" --size 800x500
```

Reads stdin until it closes and shows it in a monospaced, read-only view that follows the tail -
scrolling up stops the following, scrolling back to the bottom resumes it. `--file PATH` reads a
file instead. While input is still arriving the button says *Stop* and exits 1; once stdin is done
it turns into *Close* and exits 0.

### `--progress` - a progress window driven over D-Bus

```bash
handle=$(kurodlg --progress "Encoding…" 100 --title "AV1" --size 640x420 --details)
service=${handle%% *}
path=${handle##* }
```

`--launcher NAME` names the desktop entry whose task icon carries the progress; the default is
kurodlg's own.

Returns immediately, having printed `<service> <path>`, and leaves the window to a detached
process - the same contract as `kdialog --progressbar`. Drive it on
`net.kurokyuu.kurodlg.ProgressDialog`:

| Member | Does |
| --- | --- |
| `value`, `maximum`, `autoClose` | properties, as in kdialog |
| `setLabelText(s)` | the line above the bar |
| `showCancelButton(b)` | offer Cancel; it is hidden by default |
| `wasCancelled() → b` | whether the user asked to stop |
| `close()` | closes the window and ends the process |
| `setTitle(s)`, `setHeader(s)` | window title, and a heading inside the dialog |
| `resizeWindow(w, h)` | resize after the fact |
| `appendDetail(s)` | one line into the details area |
| `clearDetails()` | empty it |
| `setDetailsVisible(b)` | expand or collapse the details area |
| `setProgressVisible(b)` | hide the bar, leaving a plain output window |
| `setFinished()` | the work is over: the button becomes *Close* and the window waits for it |

The first seven members are kdialog's interface verbatim, so a caller written against
`org.kde.kdialog.ProgressDialog` works after changing the interface name.

**Cancel does not close the window.** The button sets the flag and then reads *Cancelling…*,
because only the caller knows how long stopping takes and whether it succeeded. Closing the window
or pressing Esc sets the same flag and does end the process - after that, calls on the handle fail,
which means "the dialog is gone", not "the user cancelled".

**`setFinished()` instead of `close()`** leaves the window standing with whatever the details area
collected, under a *Close* button. It shows that button even if `showCancelButton` never did, so a
caller that reports through `appendDetail` can end a run without the reader having to reach for the
title bar. The caller may exit afterwards; the dialog is a process of its own.

**The progress also lands on the task icon**, the bar that fills up under a Dolphin copy. Every
`value` change is announced on `com.canonical.Unity.LauncherEntry` - a Canonical interface in a KDE
panel, because that is what applications already emitted; `org.kde.plasma.taskmanager` holds that
bus name and listens for it. It is cleared by `setFinished()` and when the window closes.

The panel finds the task through its **desktop entry**, which is why `kurodlg.desktop` is installed
(`NoDisplay=true`, so it is not an entry in the application menu) and `setDesktopFileName()` is set:
without one there is no task for a bar to be drawn on, however correct the signal is.
`--launcher NAME` puts the progress on another program's icon instead - the one that started the
job, say - and that is the case the clearing matters for, since that icon outlives this process.

#### `--job` - a Plasma job instead of a window

```bash
handle=$(kurodlg --progress "Converting…" 12 --job --header "AV1" --launcher org.kde.dolphin)
```

No window at all: the progress becomes a job notification, the one Dolphin shows for a copy - with the
heading as its title, the label text under it, a bar, and *Cancel*. The D-Bus interface is the same, so
a script adds the flag and changes nothing else. Cancelling from the notification sets `wasCancelled()`,
exactly as the window's button does. `setFinished()` reports the job done; `close()` counts as cancelled
unless `value` had reached `maximum`. The bar on the task icon is Plasma's own here, so the Unity signal
is not sent; `--launcher` names the program the job is listed under.

### `--passivepopup` - a notification

```bash
kurodlg --passivepopup "Encoding finished" 5 --title "AV1" --icon video-x-generic &
```

As in kdialog: a desktop notification, with `--title` and `--icon`, closed after the optional number of
seconds. The process waits until the notification is gone, hence the `&` in a script that goes on.

### `--about` - the About page

The standard KDE About page in a window of its own: version, description, licence, where to report a
problem, and the libraries in use. The dialogs have no menu to put it in, so it is a mode. On the command
line the same data comes from `--version`, `--author` and `--license`.

## Notes

- The details area keeps the last 5000 lines. An encode writes a line per second for hours.
- The bus name comes from the launching process, not from the dialog's process id:
  `QProcess::startDetached` forks twice and the id it reports is not the one the surviving process
  runs under.
- The detached dialog's stdout and stderr go to `/dev/null`. Inherited, they would hold a command
  substitution's pipe open long after the launcher exited, and `handle=$(kurodlg --progress …)`
  would hang. To see a dialog's own output, run the internal `--progress-server <busname>` mode by
  hand.

## Licence

kurodlg is licensed under the [GNU General Public License v3.0 or later](LICENSE.md).
