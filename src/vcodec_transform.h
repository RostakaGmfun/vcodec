#pragma once

void forward4x4(int *tblock, const int *block);

void inverse4x4(int *block, const int *tblock);

void hadamard4x4(int *tblock, const int *block);

void ihadamard4x4(int *block, const int *tblock);

void hadamard2x2(int *tblock, const int *block);
