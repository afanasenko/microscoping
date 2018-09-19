#pragma once

bool ConvertArgbToRgb(char const *argb, int width, int height, std::vector<char> &ret);

bool WriterTest(uint32_t const *buffer, int width, int height);