#include <mrs/stretch.hpp>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace mrs::stretch {
Status validate(const Parameters& p) noexcept {
    const auto finite = [](double x) { return std::isfinite(x); };
    if (!finite(p.duration_ratio) || p.duration_ratio < 0.125 || p.duration_ratio > 8 ||
        !finite(p.semitones) || !finite(p.cents) || std::abs(p.cents) > 100 ||
        std::abs(p.semitones + p.cents/100) > 24 ||
        !finite(p.formant_semitones) || std::abs(p.formant_semitones) > 12 ||
        !finite(p.formant_base_hz) || (p.formant_base_hz != 0 &&
        (p.formant_base_hz < 20 || p.formant_base_hz > 2000))) return Status::invalid_parameter;
    if (p.formants != Formants::follow_pitch && p.formants != Formants::preserve &&
        p.formants != Formants::shift) return Status::unsupported;
    if (p.quality != Quality::balanced && p.quality != Quality::economical) return Status::unsupported;
    return Status::ok;
}
bool neutral(const Parameters& p) noexcept {
    return !p.enabled || (p.duration_ratio == 1 && p.semitones + p.cents/100 == 0 &&
        (p.formants != Formants::shift || p.formant_semitones == 0));
}
Status output_frames(std::int64_t source, double ratio, std::int64_t& result) noexcept {
    if (source < 0 || !std::isfinite(ratio) || ratio <= 0) return Status::invalid_parameter;
    const long double n = std::floor(static_cast<long double>(source)*ratio + 0.5L);
    if (n >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
        return Status::invalid_parameter;
    result = static_cast<std::int64_t>(n);
    return Status::ok;
}
std::uint32_t FrameDemand::peek(std::uint32_t output, double ratio) const noexcept {
    if (!std::isfinite(ratio) || ratio <= 0) return UINT32_MAX;
    const auto n = std::floor(fraction_ + static_cast<long double>(output)/ratio);
    if (n < 0 || n > UINT32_MAX) return UINT32_MAX;
    return static_cast<std::uint32_t>(n);
}
void FrameDemand::advance(std::uint32_t output, double ratio) noexcept {
    const auto n = fraction_ + static_cast<long double>(output)/ratio;
    fraction_ = n - std::floor(n);
}
std::string serialize(const Parameters& p) {
    if (validate(p) != Status::ok) throw std::invalid_argument("Invalid stretch state");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << "MRS_STRETCH 1 " << p.enabled << ' ' << p.duration_ratio << ' '
        << p.semitones << ' ' << p.cents << ' ' << static_cast<int>(p.formants) << ' '
        << p.formant_semitones << ' ' << p.formant_base_hz << ' ' << static_cast<int>(p.quality);
    return out.str();
}
Parameters deserialize(std::string_view text) {
    std::istringstream in{std::string{text}};
    in.imbue(std::locale::classic());
    Parameters p;
    std::string tag;
    int version{}, enabled{}, formants{}, quality{};
    if (!(in >> tag >> version >> enabled >> p.duration_ratio >> p.semitones >> p.cents >>
          formants >> p.formant_semitones >> p.formant_base_hz >> quality) ||
        tag != "MRS_STRETCH" || version != 1 || (enabled != 0 && enabled != 1))
        throw std::invalid_argument("Unsupported stretch state");
    in >> std::ws;
    if (!in.eof()) throw std::invalid_argument("Trailing stretch state");
    p.enabled = enabled != 0;
    p.formants = static_cast<Formants>(formants);
    p.quality = static_cast<Quality>(quality);
    if (validate(p) != Status::ok) throw std::invalid_argument("Invalid stretch state");
    return p;
}
} // namespace mrs::stretch
