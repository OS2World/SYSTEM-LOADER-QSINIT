/*---------------------------------------------------------*/
/*                                                         */
/*   Puzzle.h : Header file for puzzle.cpp                 */
/*                                                         */
/*---------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined( __PUZZLE_H )
#define __PUZZLE_H

class TPuzzleView : public TView {
public:
    TPuzzleView(TRect& r);
    virtual TPalette& getPalette() const;
    virtual void handleEvent(TEvent& event);
    virtual void draw();
    void moveKey(int key);
    void moveTile(TPoint point);
    void scramble();
    void winCheck();
private:
    char board[6][6];
    int moves;
    char solved;
};

class TPuzzleWindow : public TWindow {
public:
    TPuzzleWindow();
};

#endif // __PUZZLE_H
