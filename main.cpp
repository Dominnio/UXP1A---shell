/*
 *  Created by Dominik on 26.05.2018.
 *
 * ================= POLECAM PRZECZYTAC =================
 *
 * Jeśli ktoś ma ochotę to polecam poczytać komenatarze w plikach nagłówkowych.
 *
 * Najważniejsze jest to co niewidoczne dla oczu - taki żart (buhahah).
 *
 * Najważniejsze jest to co jest w Statement.h. Od tego też polecam zacząć.
 *
 * Nie polecam modyfikować żadnego z plików oprócz tego co napisałem w komenatrzach.
 *
 * Na razie nie mam pojęcia jak zrealizować potok, ale wyobrażam to sobie tak, że jedna metoda execute()
 * będzie drugiej metodzie execute() przekazywała swojój output na input tej drugiej. I rozumiem, że zrobi to przez potok,
 * ale co to tak naprawdę znaczy to nie wiem.
 *
 */

#include "Terminal.h"

int main()
{
    Terminal::getInstance().start();
    return 0;
}

