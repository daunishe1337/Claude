#pragma once
// Процедурные «фотографии» (пейзажи, нарисованные кодом) и доступ к изображениям файлов.

#include "../Common.h"

struct FsNode;

std::shared_ptr<Gdiplus::Bitmap> PhotoBitmap(int seed);
std::shared_ptr<Gdiplus::Bitmap> NodeBitmap(const FsNode* node);
void ClearPhotoCache();
