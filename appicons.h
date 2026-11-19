#ifndef APPICONS_H
#define APPICONS_H

#include <QIcon>

/** Icons shared by the grid tiles and the mini apps. */
namespace AppIcons
{
/// Icon for the Minesweeper tile on the main grid.
QIcon minesweeper();

/// Icon for the launcher window and the desktop entry.
QIcon launcher();

/// A flag marker, coloured for a light or dark tile.
QIcon flag(bool dark);

/// A mine marker, coloured for a light or dark tile.
QIcon mine(bool dark);
} // namespace AppIcons

#endif // APPICONS_H
