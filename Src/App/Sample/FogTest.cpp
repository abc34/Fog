#include <Fog/Core.h>
#include <Fog/G2d.h>
#include <Fog/UI.h>

using namespace Fog;

#if 1
// ============================================================================
// [FeTurbulenceContext]
// ============================================================================

// Code from W3 to generate turbulence, for testing only.
//
// Produces results in the range:
//   1...2^31 - 2.
//
// Algorithm:
//   R = (A * R) % M
//
// Where:
//   A = 16807
//   M = 2147483647 (2^31 - 1)
//
// See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988.
//
// To test:
//   The algorithm should produce the result 1043618065 as the 10,000th 
//   generated number if the original seed is 1.
//
// NOTE: Random number generator and base algorithm was kept to ensure that the
// turbulence generated by Firefox/WebKit browsers will be compatible to the
// turbulence generated by Fog-Framework.
#define FE_TURBULENCE_RAND_M 2147483647   // 2^31 - 1.
#define FE_TURBULENCE_RAND_A 16807        // 7^5; primitive root of M.
#define FE_TURBULENCE_RAND_Q 127773       // M / A.
#define FE_TURBULENCE_RAND_R 2836         // M % A.

#define FE_TURBULENCE_BSIZE 0x100
#define FE_TURBULENCE_BMASK 0xFF
#define FE_TURBULENCE_PERLIN 0x1000

#define s_curve(t) ( t * t * (3.0f - 2.0f * t) )
#define lerp(t, a, b) ( a + t * (b - a) )

struct FOG_NO_EXPORT FeTurbulenceStitchInfo
{
  // How much to subtract to wrap for stitching.
  int nWidth;
  int nHeight;

  // Minimum value to wrap.
  int nWrapX;
  int nWrapY;
};

struct FOG_NO_EXPORT FeTurbulenceContext
{
  void setupSeed(int32_t initialSeed);
  int32_t getRand();
  void init(int32_t initialSeed);

  void noise2(float* result, float vec[2], FeTurbulenceStitchInfo* pStitchInfo);
  uint32_t turbulence(float *point, float fBaseFreqX, float fBaseFreqY,
    int nNumOctaves, bool bFractalSum, bool bDoStitching,
    float fTileX, float fTileY, float fTileWidth, float fTileHeight);

  int32_t lSeed;
  int uLatticeSelector[FE_TURBULENCE_BSIZE + FE_TURBULENCE_BSIZE + 2];
  float fGradient[FE_TURBULENCE_BSIZE + FE_TURBULENCE_BSIZE + 2][8];
};


void FeTurbulenceContext::setupSeed(int32_t initialSeed)
{
  lSeed = initialSeed;

  if (lSeed <= 0)
    lSeed = -(lSeed % (FE_TURBULENCE_RAND_M - 1)) + 1;

  if (uint32_t(lSeed) > FE_TURBULENCE_RAND_M - 1)
    lSeed = FE_TURBULENCE_RAND_M - 1;
}

int32_t FeTurbulenceContext::getRand()
{
  int32_t result = FE_TURBULENCE_RAND_A * (lSeed % FE_TURBULENCE_RAND_Q) - FE_TURBULENCE_RAND_R * (lSeed / FE_TURBULENCE_RAND_Q);
  if (result <= 0)
    result += FE_TURBULENCE_RAND_M;
  lSeed = result;
  return result;
}

void FeTurbulenceContext::init(int32_t initialSeed)
{
  int i, j, k;
  setupSeed(initialSeed);

  for (i = 0; i < FE_TURBULENCE_BSIZE; i++)
  {
    for (k = 0; k < 4; k++)
    {
      uLatticeSelector[i] = i;
      for (j = 0; j < 2; j++)
        fGradient[i][j * 4 + k] = (float)((getRand() % (FE_TURBULENCE_BSIZE + FE_TURBULENCE_BSIZE)) - FE_TURBULENCE_BSIZE) / FE_TURBULENCE_BSIZE;
      float s = float(Math::sqrt(fGradient[i][0 + k] * fGradient[i][0 + k] + fGradient[i][4 + k] * fGradient[i][4 + k]));
      fGradient[i][0 + k] /= s;
      fGradient[i][4 + k] /= s;
    }
  }

  while (--i)
  {
    k = uLatticeSelector[i];
    j = getRand() % FE_TURBULENCE_BSIZE;

    uLatticeSelector[i] = uLatticeSelector[j];
    uLatticeSelector[j] = k;
  }

  for (i = 0; i < FE_TURBULENCE_BSIZE + 2; i++)
  {
    uLatticeSelector[FE_TURBULENCE_BSIZE + i] = uLatticeSelector[i];
    for (j = 0; j < 8; j++)
      fGradient[FE_TURBULENCE_BSIZE + i][j] = fGradient[i][j];
  }
}

void FOG_INLINE FeTurbulenceContext::noise2(float* result, float vec[2], FeTurbulenceStitchInfo* pStitchInfo)
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, sx, sy, t;

  t = vec[0] + FE_TURBULENCE_PERLIN;
  bx0 = (int)t;
  bx1 = bx0 + 1;
  rx0 = t - bx0;
  rx1 = rx0 - 1.0f;

  t = vec[1] + FE_TURBULENCE_PERLIN;
  by0 = (int)t;
  by1 = by0 + 1;
  ry0 = t - by0;
  ry1 = ry0 - 1.0f;

  // If stitching, adjust lattice points accordingly.
  /*
  if (pStitchInfo != NULL)
  {
    if (bx0 >= pStitchInfo->nWrapX) bx0 -= pStitchInfo->nWidth;
    if (bx1 >= pStitchInfo->nWrapX) bx1 -= pStitchInfo->nWidth;
    if (by0 >= pStitchInfo->nWrapY) by0 -= pStitchInfo->nHeight;
    if (by1 >= pStitchInfo->nWrapY) by1 -= pStitchInfo->nHeight;
  }
  */
  bx0 &= FE_TURBULENCE_BMASK;
  bx1 &= FE_TURBULENCE_BMASK;
  by0 &= FE_TURBULENCE_BMASK;
  by1 &= FE_TURBULENCE_BMASK;

  int i = uLatticeSelector[bx0];
  int j = uLatticeSelector[bx1];

  b00 = uLatticeSelector[i + by0];
  b10 = uLatticeSelector[j + by0];
  b01 = uLatticeSelector[i + by1];
  b11 = uLatticeSelector[j + by1];

  sx = float(s_curve(rx0));
  sy = float(s_curve(ry0));

  for (size_t n = 0; n < 4; n++)
  {
    float* q;
    float a, b;
    float u, v;

    q = fGradient[b00]; u = rx0 * q[n] + ry0 * q[n + 4];
    q = fGradient[b10]; v = rx1 * q[n] + ry0 * q[n + 4];
    a = lerp(sx, u, v);

    q = fGradient[b01]; u = rx0 * q[n] + ry1 * q[n + 4];
    q = fGradient[b11]; v = rx1 * q[n] + ry1 * q[n + 4];
    b = lerp(sx, u, v);

    result[n] = lerp(sy, a, b);
  }
}

uint32_t FeTurbulenceContext::turbulence(
  float *point, float fBaseFreqX, float fBaseFreqY,
  int nNumOctaves, bool bFractalSum, bool bDoStitching,
  float fTileX, float fTileY, float fTileWidth, float fTileHeight)
{
 /*
  // Not stitching when NULL.
  FeTurbulenceStitchInfo stitch;
  FeTurbulenceStitchInfo* pStitchInfo = NULL;

  // Adjust the base frequencies if necessary for stitching.
  if (bDoStitching)
  {
    // When stitching tiled turbulence, the frequencies must be adjusted
    // so that the tile borders will be continuous.
    if (fBaseFreqX != 0.0)
    {
      float fLoFreq = float(Math::floor(fTileWidth * fBaseFreqX)) / fTileWidth;
      float fHiFreq = float(Math::ceil(fTileWidth * fBaseFreqX)) / fTileWidth;

      if (fBaseFreqX / fLoFreq < fHiFreq / fBaseFreqX)
        fBaseFreqX = fLoFreq;
      else
        fBaseFreqX = fHiFreq;
    }

    if (fBaseFreqY != 0.0)
    {
      float fLoFreq = float(Math::floor(fTileHeight * fBaseFreqY)) / fTileHeight;
      float fHiFreq = float(Math::ceil(fTileHeight * fBaseFreqY)) / fTileHeight;

      if (fBaseFreqY / fLoFreq < fHiFreq / fBaseFreqY)
        fBaseFreqY = fLoFreq;
      else
        fBaseFreqY = fHiFreq;
    }

    // Set up initial stitch values.
    pStitchInfo = &stitch;
    stitch.nWidth = int(fTileWidth * fBaseFreqX + 0.5f);
    stitch.nWrapX = fTileX * fBaseFreqX + FE_TURBULENCE_PERLIN + stitch.nWidth;
    stitch.nHeight = int(fTileHeight * fBaseFreqY + 0.5f);
    stitch.nWrapY = fTileY * fBaseFreqY + FE_TURBULENCE_PERLIN + stitch.nHeight;
  }
*/
  float fSum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  float ratio = 1;
  float vec[2];

  vec[0] = point[0] * fBaseFreqX;
  vec[1] = point[1] * fBaseFreqY;

  int nOctave = 0;
  for (;;)
  {
    float noise[4];
    //noise2(noise, vec, pStitchInfo);
    noise2(noise, vec, NULL);

    if (bFractalSum)
    {
      fSum[0] += noise[0] * ratio;
      fSum[1] += noise[1] * ratio;
      fSum[2] += noise[2] * ratio;
      fSum[3] += noise[3] * ratio;
    }
    else
    {
      fSum[0] += Math::abs(noise[0] * ratio);
      fSum[1] += Math::abs(noise[1] * ratio);
      fSum[2] += Math::abs(noise[2] * ratio);
      fSum[3] += Math::abs(noise[3] * ratio);
    }

    if (++nOctave >= nNumOctaves)
      break;

    vec[0] *= 2.0f;
    vec[1] *= 2.0f;
    ratio *= 0.5f;
/*
    if (pStitchInfo != NULL)
    {
      // Update stitch values. Subtracting FE_TURBULENCE_PERLIN before the 
      // multiplication and adding it afterward simplifies to subtracting
      // it once.
      stitch.nWidth *= 2;
      stitch.nWrapX = 2 * stitch.nWrapX - FE_TURBULENCE_PERLIN;
      stitch.nHeight *= 2;
      stitch.nWrapY = 2 * stitch.nWrapY - FE_TURBULENCE_PERLIN;
    }
*/
  }

  if (bFractalSum)
  {
    fSum[0] = fSum[0] * 0.5f + 0.5f;
    fSum[1] = fSum[1] * 0.5f + 0.5f;
    fSum[2] = fSum[2] * 0.5f + 0.5f;
    fSum[3] = fSum[3] * 0.5f + 0.5f;
  }

  fSum[0] = Math::bound<float>(fSum[0], 0.0f, 1.0f);
  fSum[1] = Math::bound<float>(fSum[1], 0.0f, 1.0f);
  fSum[2] = Math::bound<float>(fSum[2], 0.0f, 1.0f);
  fSum[3] = Math::bound<float>(fSum[3], 0.0f, 1.0f);

  fSum[3] *= 255.0f;
  fSum[0] *= fSum[3];
  fSum[1] *= fSum[3];
  fSum[2] *= fSum[3];

  return Argb32(int(fSum[3]), int(fSum[0]), int(fSum[1]), int(fSum[2]));
}

static Image makeTurbulence(FeTurbulence* feData)
{
  Image image;

  FeTurbulenceContext ctx;
  ctx.init(feData->getSeed());

  image.create(SizeI(500, 500), IMAGE_FORMAT_PRGB32);
  if (image.isEmpty())
    return image;

  int x, y;
  int w, h;
  
  w = image.getWidth();
  h = image.getHeight();

  uint8_t* p = image.getFirstX();
  ssize_t stride = image.getStride();

  for (y = 0; y < h; y++)
  {
    for (x = 0; x < w; x++)
    {
      float pt[2] = { float(x), float(y) };

      uint32_t pix = ctx.turbulence(pt,
        feData->getHorizontalBaseFrequency(),
        feData->getVerticalBaseFrequency(),
        feData->getNumOctaves(),
        feData->getTurbulenceType(),
        feData->getStitchTitles(),
        float(x), float(y), float(w), float(h));
      
      reinterpret_cast<uint32_t*>(p)[x] = pix;
    }

    p += stride;
  }

  return image;
}
#endif

#if 1
static Image makeSpiral()
{
  Image image;
  int x, y;
  int w = 400, h = 400;

  image.create(SizeI(w, h), IMAGE_FORMAT_PRGB32);

  uint8_t* data = image.getFirstX();
  ssize_t stride = image.getStride();

  float cx = 200.0f;
  float cy = 200.0f;

  for (y = 0; y < h; y++, data += stride)
  {
    uint8_t* p = data;

    for (x = 0; x < w; x++, p += 4)
    {
      float px = static_cast<float>(x);
      float py = static_cast<float>(y);

      float d;

      //d = Math::atan2(px - cx, py - cy) / float(MATH_TWO_PI) + 0.5f;
      d = Math::atan2(px - cx, py - cy) / float(MATH_TWO_PI);
      d += Math::sqrt(Math::pow2(px - cx) + Math::pow2(py - cy)) * 0.005f;

      d = Math::positiveFraction(d);

      uint32_t pix = (int(d * 255.0f) * 0x010101) | 0xFF000000;
      Acc::p32Store4a(p, pix);
    }
  }

  return image;
}
#endif

// ============================================================================
// [SampleWindow - Declaration]
// ============================================================================

struct AppWindow : public UIEngineWindow
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AppWindow(UIEngine* engine, uint32_t hints = 0);
  virtual ~AppWindow();

  // --------------------------------------------------------------------------
  // [Event Handlers]
  // --------------------------------------------------------------------------

  virtual void onEngineEvent(UIEngineEvent* ev);
  virtual void onPaint(Painter* p);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------
  
  Image background;
};

// ============================================================================
// [SampleWindow - Construction / Destruction]
// ============================================================================

AppWindow::AppWindow(UIEngine* engine, uint32_t hints) :
  UIEngineWindow(engine, hints)
{
  background.create(SizeI(40, 40), IMAGE_FORMAT_XRGB32);
  background.fillRect(RectI( 0,  0, 20, 20), Argb32(0xFFFFFFFF));
  background.fillRect(RectI(20,  0, 20, 20), Argb32(0xFFCFCFCF));
  background.fillRect(RectI( 0, 20, 20, 20), Argb32(0xFFCFCFCF));
  background.fillRect(RectI(20, 20, 20, 20), Argb32(0xFFFFFFFF));
}

AppWindow::~AppWindow()
{
}

// ============================================================================
// [SampleWindow - Event Handlers]
// ============================================================================

void AppWindow::onEngineEvent(UIEngineEvent* ev)
{
  switch (ev->getCode())
  {
    case UI_ENGINE_EVENT_CLOSE:
      Application::get()->quit();
      break;

    case UI_ENGINE_EVENT_PAINT:
      onPaint(static_cast<UIEnginePaintEvent*>(ev)->getPainter());
      break;
  }
}

void AppWindow::onPaint(Painter* _p)
{
  Painter& p = *_p;
  RectI geom = getClientGeometry();

  p.setSource(Texture(background));
  //p.setSource(Argb32(0xFFFFFFFF));
  p.fillAll();

  TimeTicks startTime = TimeTicks::now();

#if 0
  {
    FeTurbulence fe;
    fe.setBaseFrequency(0.007f);
    fe.setNumOctaves(2);
    fe.setTurbulenceType(FE_TURBULENCE_TYPE_FRACTAL_NOISE);
    fe.setSeed(1000);

    Image image = makeTurbulence(&fe);
    p.blitImage(PointI(0, 0), image);
  }
#endif

#if 0
  {
    Image image = makeSpiral();
    p.blitImage(PointI(0, 0), image);
  }
#endif

  Font font;
  //font.create(StringW::fromAscii8("Arial Hebrew"), 16.0f);
  PointF pt(100.0f, 5.0f);

  p.setSource(Argb32(0xFF000000));
  font.setSize(48);

#if 0
  FaceCollection collection;
  FontEngine::getGlobal()->getAvailableFaces(collection);

  ListIterator<FaceInfo> it(collection.getList());
  while (it.isValid())
  {
    StringA loc;
    TextCodec::utf8().encode(loc, it.getItem().getFamilyName());
    Logger::info("", "main", "%s\n", loc.getData());

    font.create(it.getItem().getFamilyName(), 15.0f);
    pt.y += font.getAscent();

    StringW str = it.getItem().getFamilyName();
    p.fillText(pt, str, font);

    pt.y += font.getDescent();
    it.next();
  }
#endif

#if 1
  for (size_t i = 0; i < 18; i++)
  {
    StringW str(Ascii8("Sample text, VA AV, 1234567890"));
    p.fillText(pt, str, font);

    pt.y += font.getDescent();
    font.setSize(font.getSize() - 2.0f);
    pt.y += font.getAscent();
  }
#endif

#if 0
  LinearGradientF gradient;
  gradient.setStart(0.0f, 0.0f);
  gradient.setEnd(200.0f, 200.0f);
  gradient.addStop(0.0f, Argb32(0xFFFFFFFF));
  gradient.addStop(1.0f, Argb32(0x00FFFFFF));
  p.setSource(gradient);
  p.fillRect(RectI(0, 0, 200, 200));
#endif

#if 0
  Image mask;
  mask.create(SizeI(300, 300), IMAGE_FORMAT_PRGB32);

  Painter mPainter(mask);
  mPainter.setCompositingOperator(COMPOSITE_SRC);
  mPainter.setSource(Argb32(0x00000000));
  mPainter.fillAll();
  mPainter.setSource(Argb32(0xFF000000));
  mPainter.fillTriangle(TriangleF(0.0f, 0.5f, 298.0f, 3.0f, 150.5f, 299.0f));
  mPainter.end();

  mask.convert(IMAGE_FORMAT_A8);
  p.setSource(Argb32(0xFF000000));
  p.fillAll();
  p.blitImage(PointI(100, 100), mask);
#endif

  TimeDelta t = TimeTicks::now() - startTime;

  StringW text;
  text.format("Render: %g [ms]", t.getMillisecondsD());
  setWindowTitle(text);
}

// ============================================================================
// [FOG_UI_MAIN]
// ============================================================================

FOG_UI_MAIN()
{
  Application app(StringW::fromAscii8("UI"));
  AppWindow wnd(app.getUIEngine());

  wnd.setWindowTitle(StringW::fromAscii8("FogTest"));
  wnd.setWindowSize(SizeI(890, 695));
  wnd.show();

  return app.run();
}
