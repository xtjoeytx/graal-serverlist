//
// Created by Joseph Fichera on 2019-06-05.
//

#ifndef LISTSERVER_PLAYER_H
#define LISTSERVER_PLAYER_H

#include <CString.h>

class Player
{
	CString account, nick, level;
	float x, y;
	unsigned char ap, type;
};

#endif //LISTSERVER_PLAYER_H
