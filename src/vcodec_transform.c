/**
 * Integer transform and hadamard transform implementation from reference H.264 implementation.
 */

void forward4x4(int *tblock, const int *block)
{
  int i;
  int tmp[16];
  int *pTmp = tmp;
  const int*pblock;
  int p0,p1,p2,p3;
  int t0,t1,t2,t3;

  // Horizontal
  for (i=0; i < 4; i++)
  {
    pblock = &block[i * 4];
    p0 = *(pblock++);
    p1 = *(pblock++);
    p2 = *(pblock++);
    p3 = *(pblock  );

    t0 = p0 + p3;
    t1 = p1 + p2;
    t2 = p1 - p2;
    t3 = p0 - p3;

    *(pTmp++) =  t0 + t1;
    *(pTmp++) = (t3 << 1) + t2;
    *(pTmp++) =  t0 - t1;
    *(pTmp++) =  t3 - (t2 << 1);
  }

  // Vertical
  for (i=0; i < 4; i++)
  {
    pTmp = tmp + i;
    p0 = *pTmp;
    p1 = *(pTmp += 4);
    p2 = *(pTmp += 4);
    p3 = *(pTmp += 4);

    t0 = p0 + p3;
    t1 = p1 + p2;
    t2 = p1 - p2;
    t3 = p0 - p3;

    tblock[i] = t0 +  t1;
    tblock[4  + i] = t2 + (t3 << 1);
    tblock[8  + i] = t0 -  t1;
    tblock[12 + i] = t3 - (t2 << 1);
  }
}

void inverse4x4(int *block, const int *tblock)
{
  int i;
  int tmp[16];
  int *pTmp = tmp;
  const int *pblock;
  int p0,p1,p2,p3;
  int t0,t1,t2,t3;

  // Horizontal
  for (i = 0; i < 4; i++)
  {
    pblock = &tblock[i * 4];
    t0 = *(pblock++);
    t1 = *(pblock++);
    t2 = *(pblock++);
    t3 = *(pblock  );

    p0 =  t0 + t2;
    p1 =  t0 - t2;
    p2 = (t1 >> 1) - t3;
    p3 =  t1 + (t3 >> 1);

    *(pTmp++) = p0 + p3;
    *(pTmp++) = p1 + p2;
    *(pTmp++) = p1 - p2;
    *(pTmp++) = p0 - p3;
  }

  //  Vertical
  for (i = 0; i < 4; i++)
  {
    pTmp = tmp + i;
    t0 = *pTmp;
    t1 = *(pTmp += 4);
    t2 = *(pTmp += 4);
    t3 = *(pTmp += 4);

    p0 = t0 + t2;
    p1 = t0 - t2;
    p2 =(t1 >> 1) - t3;
    p3 = t1 + (t3 >> 1);

    block[i] = p0 + p3;
    block[4 + i] = p1 + p2;
    block[8 + i] = p1 - p2;
    block[12 + i] = p0 - p3;
  }
}

void hadamard4x4(int *tblock, const int *block)
{
  int i;
  int tmp[16];
  int *pTmp = tmp;
  const int *pblock;
  int p0,p1,p2,p3;
  int t0,t1,t2,t3;

  // Horizontal
  for (i = 0; i < 4; i++)
  {
    pblock = block + i * 4;
    p0 = *(pblock++);
    p1 = *(pblock++);
    p2 = *(pblock++);
    p3 = *(pblock  );

    t0 = p0 + p3;
    t1 = p1 + p2;
    t2 = p1 - p2;
    t3 = p0 - p3;

    *(pTmp++) = t0 + t1;
    *(pTmp++) = t3 + t2;
    *(pTmp++) = t0 - t1;
    *(pTmp++) = t3 - t2;
  }

  // Vertical
  for (i = 0; i < 4; i++)
  {
    pTmp = tmp + i;
    p0 = *pTmp;
    p1 = *(pTmp += 4);
    p2 = *(pTmp += 4);
    p3 = *(pTmp += 4);

    t0 = p0 + p3;
    t1 = p1 + p2;
    t2 = p1 - p2;
    t3 = p0 - p3;

    tblock[0 + i] = (t0 + t1) >> 1;
    tblock[4 + i] = (t2 + t3) >> 1;
    tblock[8 + i] = (t0 - t1) >> 1;
    tblock[12 + i] = (t3 - t2) >> 1;
  }
}


void ihadamard4x4(int *block, const int *tblock)
{
  int i;
  int tmp[16];
  int *pTmp = tmp;
  const int *pblock;
  int p0,p1,p2,p3;
  int t0,t1,t2,t3;

  // Horizontal
  for (i = 0; i < 4; i++)
  {
    pblock = tblock + i * 4;
    t0 = *(pblock++);
    t1 = *(pblock++);
    t2 = *(pblock++);
    t3 = *(pblock  );

    p0 = t0 + t2;
    p1 = t0 - t2;
    p2 = t1 - t3;
    p3 = t1 + t3;

    *(pTmp++) = p0 + p3;
    *(pTmp++) = p1 + p2;
    *(pTmp++) = p1 - p2;
    *(pTmp++) = p0 - p3;
  }

  //  Vertical
  for (i = 0; i < 4; i++)
  {
    pTmp = tmp + i;
    t0 = *pTmp;
    t1 = *(pTmp += 4);
    t2 = *(pTmp += 4);
    t3 = *(pTmp += 4);

    p0 = t0 + t2;
    p1 = t0 - t2;
    p2 = t1 - t3;
    p3 = t1 + t3;

    block[0 + i] = p0 + p3;
    block[4 + i] = p1 + p2;
    block[8 + i] = p1 - p2;
    block[12 + i] = p0 - p3;
  }
}

void hadamard2x2(int *tblock, const int *block)
{
  int p0,p1,p2,p3;

  p0 = block[0] + block[1];
  p1 = block[0] - block[1];
  p2 = block[2] + block[3];
  p3 = block[2] - block[3];

  tblock[0] = (p0 + p2);
  tblock[1] = (p1 + p3);
  tblock[2] = (p0 - p2);
  tblock[3] = (p1 - p3);
}
