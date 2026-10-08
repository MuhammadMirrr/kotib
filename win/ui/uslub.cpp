// Dizayn tokenlari va Direct2D chizish yordamchilari.
// Interfeys va izohlar — `uslub.h`.

#include "uslub.h"

#include <algorithm>

namespace rubai {

namespace {

// 0xRRGGBB → Direct2D rangi. Ilovada shaffoflik ishlatilmaydi (dizaynda yoʻq),
// shuning uchun alfa doim 1.
D2D1_COLOR_F d2dRang(uint32_t hex) { return D2D1::ColorF(hex, 1.0f); }

DWRITE_FONT_WEIGHT dwOgirlik(Ogirlik o) {
    switch (o) {
        case Ogirlik::Yarim: return DWRITE_FONT_WEIGHT_SEMI_BOLD;
        case Ogirlik::Qalin: return DWRITE_FONT_WEIGHT_BOLD;
        default: return DWRITE_FONT_WEIGHT_NORMAL;
    }
}

}  // namespace

Chizgich::~Chizgich() { resurslarniTashla(); }

bool Chizgich::qur(HWND oyna) {
    oyna_ = oyna;

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.yozish()))) {
        return false;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(dw_.yozish())))) {
        return false;
    }

    // Punktir uslubi — drop zonasi uchun. D2D1_DASH_STYLE_DASH standart
    // nisbatlari dizayndagidan zichroq, shuning uchun oʻz naqshimiz.
    const float naqsh[] = {4.0f, 3.0f};
    d2d_->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                        D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND,
                                                        10.0f, D2D1_DASH_STYLE_CUSTOM, 0.0f),
                            naqsh, ARRAYSIZE(naqsh), punktirUslub_.yozish());

    return qurilmaResurslari();
}

bool Chizgich::qurilmaResurslari() {
    if (rt_) return true;

    RECT r{};
    GetClientRect(oyna_, &r);

    // MUHIM: render target'ga oynaning DPI'si beriladi. Shundan keyin barcha
    // koordinatalar DIP'da boʻladi va 150% masshtabdagi ekranda kod hech
    // narsani koʻpaytirmasdan toʻgʻri chiqadi. Bu macOS'dagi "point"
    // tizimining aynan ekvivalenti.
    const UINT dpi = GetDpiForWindow(oyna_);

    auto xossalar = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN), static_cast<float>(dpi),
        static_cast<float>(dpi));

    auto oynaXossalari = D2D1::HwndRenderTargetProperties(
        oyna_, D2D1::SizeU(static_cast<UINT>(r.right), static_cast<UINT>(r.bottom)),
        // PRESENT_OPTIONS_NONE — vsync bilan. Diktovka ilovasida animatsiya
        // kam, immediate rejim faqat elektr sarflaydi.
        D2D1_PRESENT_OPTIONS_NONE);

    return SUCCEEDED(d2d_->CreateHwndRenderTarget(xossalar, oynaXossalari, rt_.yozish()));
}

void Chizgich::resurslarniTashla() {
    for (auto& [_, m] : moyqalamlar_)
        if (m) m->Release();
    moyqalamlar_.clear();
    for (auto& [_, s] : shakllar_)
        if (s) s->Release();
    shakllar_.clear();
    rt_.bosat();
}

void Chizgich::olchamOzgardi(UINT kenglik, UINT balandlik) {
    if (rt_) rt_->Resize(D2D1::SizeU(kenglik, balandlik));
}

void Chizgich::dpiOzgardi(UINT dpi) {
    if (rt_) rt_->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
}

void Chizgich::chizishBoshlandi() {
    if (!qurilmaResurslari()) return;
    rt_->BeginDraw();
}

bool Chizgich::chizishTugadi() {
    if (!rt_) return false;
    const HRESULT hr = rt_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        // Videokarta yoʻqoldi (drayver yangilandi, ekran uzildi). Hamma
        // qurilmaga bogʻliq resurslar yaroqsiz — tashlaymiz va chaqiruvchi
        // oynani qayta chizishga qoʻyadi.
        resurslarniTashla();
        return false;
    }
    return SUCCEEDED(hr);
}

D2D1_SIZE_F Chizgich::olcham() const { return rt_ ? rt_->GetSize() : D2D1::SizeF(0, 0); }

ID2D1SolidColorBrush* Chizgich::moyqalam(uint32_t rang) {
    auto it = moyqalamlar_.find(rang);
    if (it != moyqalamlar_.end()) return it->second;

    ID2D1SolidColorBrush* m = nullptr;
    if (FAILED(rt_->CreateSolidColorBrush(d2dRang(rang), &m))) return nullptr;
    moyqalamlar_[rang] = m;
    return m;
}

IDWriteTextFormat* Chizgich::shakl(float olcham, Ogirlik ogirlik, Hizalash hizalash, bool bitta) {
    // Kesh kaliti: oʻlcham (0.5 aniqlikda) + ogʻirlik + hizalash + kesish.
    // Kesish alohida kalit boʻlishi SHART: shakl obyekti keshda boʻlishiladi
    // va unga kesish qoʻyilsa, oʻsha oʻlcham/ogʻirlikdagi HAMMA matn
    // kesiladigan boʻlib qolardi.
    const uint64_t kalit = static_cast<uint64_t>(olcham * 2) * 1000 +
                           static_cast<uint64_t>(ogirlik) * 100 +
                           static_cast<uint64_t>(hizalash) * 10 + (bitta ? 1 : 0);
    auto it = shakllar_.find(kalit);
    if (it != shakllar_.end()) return it->second;

    IDWriteTextFormat* f = nullptr;
    // Segoe UI Variable — Windows 11 ning tizim shrifti. Windows 10 da u yoʻq
    // va DirectWrite oʻzi Segoe UI ga tushadi.
    if (FAILED(dw_->CreateTextFormat(L"Segoe UI Variable Text", nullptr, dwOgirlik(ogirlik),
                                     DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, olcham,
                                     L"", &f))) {
        return nullptr;
    }
    f->SetTextAlignment(hizalash == Hizalash::Markaz ? DWRITE_TEXT_ALIGNMENT_CENTER
                        : hizalash == Hizalash::Ong  ? DWRITE_TEXT_ALIGNMENT_TRAILING
                                                     : DWRITE_TEXT_ALIGNMENT_LEADING);

    if (bitta) {
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        Com<IDWriteInlineObject> uchNuqta;
        if (SUCCEEDED(dw_->CreateEllipsisTrimmingSign(f, uchNuqta.yozish()))) {
            DWRITE_TRIMMING kesish{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            f->SetTrimming(&kesish, uchNuqta.get());
        }
    }

    shakllar_[kalit] = f;
    return f;
}

void Chizgich::toldir(const D2D1_RECT_F& r, uint32_t rang, float radius) {
    auto* m = moyqalam(rang);
    if (!m) return;
    if (radius > 0.0f)
        rt_->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), m);
    else
        rt_->FillRectangle(r, m);
}

void Chizgich::chegara(const D2D1_RECT_F& r, uint32_t rang, float radius, float qalinlik) {
    auto* m = moyqalam(rang);
    if (!m) return;
    // Chegara chizigʻining markazi piksel chetida turmasligi uchun yarim
    // qalinlikka ichkariga suramiz — aks holda u xiralashib koʻrinadi.
    const float d = qalinlik / 2.0f;
    const D2D1_RECT_F ich = D2D1::RectF(r.left + d, r.top + d, r.right - d, r.bottom - d);
    if (radius > 0.0f)
        rt_->DrawRoundedRectangle(D2D1::RoundedRect(ich, radius, radius), m, qalinlik);
    else
        rt_->DrawRectangle(ich, m, qalinlik);
}

void Chizgich::punktirChegara(const D2D1_RECT_F& r, uint32_t rang, float radius, float qalinlik) {
    auto* m = moyqalam(rang);
    if (!m) return;
    const float d = qalinlik / 2.0f;
    const D2D1_RECT_F ich = D2D1::RectF(r.left + d, r.top + d, r.right - d, r.bottom - d);
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(ich, radius, radius), m, qalinlik,
                              punktirUslub_.get());
}

void Chizgich::aylantir(float gradus, float cx, float cy) {
    if (rt_) rt_->SetTransform(D2D1::Matrix3x2F::Rotation(gradus, D2D1::Point2F(cx, cy)));
}

void Chizgich::aylantirishniBekor() {
    if (rt_) rt_->SetTransform(D2D1::Matrix3x2F::Identity());
}

void Chizgich::chiziq(float x1, float y1, float x2, float y2, uint32_t rang, float qalinlik) {
    auto* m = moyqalam(rang);
    if (!m) return;
    rt_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), m, qalinlik);
}

void Chizgich::doira(float x, float y, float radius, uint32_t rang) {
    auto* m = moyqalam(rang);
    if (!m) return;
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), m);
}

void Chizgich::matn(const std::wstring& s, const D2D1_RECT_F& r, uint32_t rang, float olcham,
                    Ogirlik ogirlik, Hizalash hizalash, bool vertikalMarkaz, bool bitta) {
    auto* m = moyqalam(rang);
    auto* f = shakl(olcham, ogirlik, hizalash, bitta);
    if (!m || !f) return;

    f->SetParagraphAlignment(vertikalMarkaz ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                                            : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    rt_->DrawText(s.c_str(), static_cast<UINT32>(s.size()), f, r, m, D2D1_DRAW_TEXT_OPTIONS_NONE);
}

D2D1_SIZE_F Chizgich::matnOlchami(const std::wstring& s, float olcham, Ogirlik ogirlik,
                                  float maksKenglik) {
    auto* f = shakl(olcham, ogirlik, Hizalash::Chap);
    if (!f || !dw_) return D2D1::SizeF(0, 0);

    Com<IDWriteTextLayout> joylashuv;
    if (FAILED(dw_->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), f, maksKenglik, 1e6f,
                                     joylashuv.yozish()))) {
        return D2D1::SizeF(0, 0);
    }
    DWRITE_TEXT_METRICS o{};
    joylashuv->GetMetrics(&o);
    return D2D1::SizeF(o.width, o.height);
}

void Chizgich::kesishBoshla(const D2D1_RECT_F& r) {
    if (rt_) rt_->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void Chizgich::kesishTugat() {
    if (rt_) rt_->PopAxisAlignedClip();
}

}  // namespace rubai
