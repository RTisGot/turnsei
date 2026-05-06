#pragma once
#include <string>

//-宣言--
extern char g_playerName[32] ; // 入力用バッファ
extern bool g_isNamingPhase ;   // 名前入力中かどうか

void StoryEvent();
//Jsonfileを読み込ませる関数
void LoadStoryData();

//ストーリーを更新する処理を行う関数
void UpdateStory();

//ストーリーを画面に描画する処理
void DrawStory();
//bool IsStoryFinished();

