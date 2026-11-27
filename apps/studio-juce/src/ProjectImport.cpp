#include "ProjectImport.h"
#include "LiveStudioOneShowReader.h"
#include <juce_core/juce_core.h>
// Reuse the zlib implementation already built into the pinned JUCE SDK.
#include <juce_core/zip/juce_zlib.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace mrs::project_import {
namespace {
using namespace std::string_view_literals;
constexpr std::uint64_t MiB = 1024 * 1024;
constexpr auto maxXml = 8 * MiB;
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

bool validUtf8(std::string_view s) {
    return s.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0) > 0;
}

std::string pathKey(std::string_view name, bool directory = false) {
    if (name.empty() || name.size() > 1024 || !validUtf8(name)) fail("Invalid ZIP or media path");
    if (directory) {
        if (name.back() != '/') fail("Invalid ZIP directory path");
        name.remove_suffix(1);
    }
    auto rest = name;
    for (;;) {
        const auto slash = rest.find('/');
        const auto part = rest.substr(0, slash);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' '
            || part.find_first_of("\\:*?\"<>|") != part.npos
            || std::any_of(part.begin(),part.end(),[](unsigned char c){ return c < 32 || c == 127; }))
            fail("Unsafe ZIP or media path (use a relative, forward-slash path)");
        const auto stem = juce::String::fromUTF8(part.data(), static_cast<int>(part.size())).upToFirstOccurrenceOf(".", false, false).trimEnd().toUpperCase();
        if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" || stem == "CONIN$" || stem == "CONOUT$"
            || ((stem.startsWith("COM") || stem.startsWith("LPT")) && stem.length() == 4
                && juce::String::fromUTF8("123456789\xc2\xb9\xc2\xb2\xc2\xb3").containsChar(stem[3])))
            fail("Reserved device name in ZIP or media path");
        if (slash == rest.npos) break;
        rest.remove_prefix(slash + 1);
    }
    return juce::String::fromUTF8(name.data(), static_cast<int>(name.size())).toLowerCase().toStdString();
}

std::uint32_t le(std::string_view bytes, std::size_t offset, unsigned count) {
    if (offset > bytes.size() || count > bytes.size() - offset) fail("Truncated ZIP header");
    std::uint32_t result = 0;
    for (unsigned i = 0; i < count; ++i) result |= std::uint32_t(static_cast<unsigned char>(bytes[offset + i])) << (8 * i);
    return result;
}

std::string inflateXml(const std::string& compressed, std::size_t expected) {
    // Extra byte detects a member that exceeds its declared size.
    std::string output(expected + 1,'\0');
    z_stream stream{};
    if (inflateInit2(&stream,-MAX_WBITS) != Z_OK) fail("Cannot initialize ZIP inflater");
    struct Finish { z_stream& stream; ~Finish(){ inflateEnd(&stream); } } finish{stream};
    stream.next_in=reinterpret_cast<const Bytef*>(compressed.data());
    stream.avail_in=static_cast<uInt>(compressed.size());
    stream.next_out=reinterpret_cast<Bytef*>(output.data());
    stream.avail_out=static_cast<uInt>(output.size());
    if (inflate(&stream,Z_FINISH) != Z_STREAM_END || stream.avail_in != 0 || stream.total_out != expected)
        fail("Malformed or truncated ZIP XML stream");
    output.resize(expected);
    return output;
}

void checkCancelled(const std::stop_token& stop) {
    if (stop.stop_requested()) fail("DAWproject audio copy cancelled");
}

// Single-file ZIP32, stored or deflated. Inspect directory and local ranges once,
// using a single read-only file handle. P1 preview only inflates XML.
class Archive {
    struct Entry {
        std::string name;
        std::uint64_t start{};
        std::uint32_t packed{}, size{}, crc{}, method{};
        bool directory{};
    };
    std::ifstream input;
    std::map<std::string, Entry> entries;
    std::string at(std::uint64_t offset, std::size_t count) {
        std::string bytes(count, '\0');
        input.seekg(static_cast<std::streamoff>(offset));
        if (!input.read(bytes.data(), static_cast<std::streamsize>(count))) fail("Cannot read ZIP data");
        return bytes;
    }
public:
    explicit Archive(const std::filesystem::path& file, std::stop_token stop = {}) : input(file, std::ios::binary | std::ios::ate) {
        checkCancelled(stop);
        const auto end = input.tellg();
        if (end < 22 || end > static_cast<std::streamoff>(0xffffffffU)) fail("Cannot read DAWproject ZIP, or ZIP32 size limit exceeded");
        const auto size = static_cast<std::uint64_t>(end);
        const auto tailSize = static_cast<std::size_t>(std::min<std::uint64_t>(size, 65535 + 22));
        const auto tail = at(size - tailSize, tailSize);
        auto e = tail.size() - 22;
        while (!(le(tail,e,4) == 0x06054b50 && e + 22 + le(tail,e+20,2) == tail.size())) {
            if (e == 0) fail("Invalid ZIP central directory");
            --e;
        }
        const auto count = le(tail,e+10,2), length = le(tail,e+12,4), central = le(tail,e+16,4);
        if (le(tail,e+4,4) != 0 || le(tail,e+8,2) != count || count == 0 || count > 8192
            || length > 16 * MiB || std::uint64_t(central) + length != size - tailSize + e)
            fail("Unsupported ZIP layout or central-directory limit exceeded");
        const auto directory = at(central, length);
        std::size_t p = 0;
        std::uint64_t total = 0;
        std::vector<std::pair<std::uint64_t,std::uint64_t>> ranges;
        for (std::uint32_t i = 0; i < count; ++i) {
            checkCancelled(stop);
            if (le(directory,p,4) != 0x02014b50) fail("Malformed ZIP central directory");
            const auto flags = le(directory,p+8,2), method = le(directory,p+10,2);
            const auto crc = le(directory,p+16,4), packed = le(directory,p+20,4), unpacked = le(directory,p+24,4);
            const auto nameLen = le(directory,p+28,2), extra = le(directory,p+30,2), comment = le(directory,p+32,2);
            const auto mode = le(directory,p+38,4) >> 16, local = le(directory,p+42,4);
            if ((flags & ~0x080eU) != 0 || (method != 0 && method != 8) || le(directory,p+34,2) != 0
                || (mode & 0170000U) == 0120000U || (method == 0 && packed != unpacked)
                || p + 46 + nameLen + extra + comment > directory.size()) fail("Unsupported or malformed ZIP entry");
            auto name = directory.substr(p+46,nameLen);
            const bool isDir = !name.empty() && name.back() == '/';
            auto key = pathKey(name, isDir);
            p += 46 + nameLen + extra + comment;
            total += unpacked;
            if (unpacked > 2048 * MiB || total > 8 * 1024 * MiB || (isDir && unpacked != 0)) fail("ZIP expanded-size limit exceeded");
            if (std::uint64_t(local) + 30 + nameLen > central) fail("Invalid ZIP local header range");
            const auto header = at(local, 30 + nameLen);
            if (le(header,0,4) != 0x04034b50 || le(header,6,2) != flags || le(header,8,2) != method
                || le(header,26,2) != nameLen || header.compare(30,nameLen,name) != 0)
                fail("ZIP local header disagrees with central directory");
            if ((flags & 8U) == 0 && (le(header,14,4) != crc || le(header,18,4) != packed || le(header,22,4) != unpacked))
                fail("ZIP local size or checksum disagrees with central directory");
            const auto start = std::uint64_t(local) + 30 + nameLen + le(header,28,2);
            auto memberEnd = start + packed;
            if (memberEnd > central) fail("Invalid ZIP member range");
            if (flags & 8U) {
                if (central - memberEnd < 12) fail("Truncated ZIP descriptor");
                const auto descriptor = at(memberEnd, central - memberEnd >= 16 ? 16 : 12);
                const auto matches = [&](std::size_t n) { return le(descriptor,n,4) == crc && le(descriptor,n+4,4) == packed && le(descriptor,n+8,4) == unpacked; };
                if (descriptor.size() == 16 && le(descriptor,0,4) == 0x08074b50 && matches(4)) memberEnd += 16;
                else if (matches(0)) memberEnd += 12;
                else fail("Invalid ZIP descriptor");
            }
            if (!entries.emplace(std::move(key), Entry{std::move(name),start,packed,unpacked,crc,method,isDir}).second)
                fail("Duplicate ZIP entry name");
            ranges.emplace_back(local,memberEnd);
        }
        if (p != directory.size()) fail("Unexpected data in ZIP directory");
        std::sort(ranges.begin(),ranges.end());
        for (std::size_t i=1;i<ranges.size();++i) if (ranges[i].first < ranges[i-1].second) fail("Overlapping ZIP entries");
        for (const auto& [key, record] : entries) {
            checkCancelled(stop);
            for (auto slash = key.find('/'); slash != key.npos; slash = key.find('/',slash+1)) {
                if (const auto part = entries.find(key.substr(0,slash)); part != entries.end() && !part->second.directory)
                    fail("ZIP path refers to a file as a directory");
            }
        }
    }
    bool contains(std::string_view path) const {
        const auto found = entries.find(pathKey(path));
        return found != entries.end() && !found->second.directory && found->second.name == path;
    }
    // The same ZIP bounds, exact member identity and CRC apply to binary
    // performance data. Never extract or open an archived external path.
    std::optional<std::string> member(std::string_view name,std::uint64_t maxBytes=maxXml) {
        const auto it = entries.find(pathKey(name));
        if (it == entries.end()) return {};
        const auto& record = it->second;
        if (record.name != name || record.directory || record.size == 0 || record.size > maxBytes || record.packed > maxBytes + 65536
            || record.packed == 0 || record.size > std::uint64_t(record.packed) * 512 + 65536)
            fail("Invalid ZIP member or expansion limit exceeded");
        auto compressed = at(record.start, record.packed);
        auto bytes = record.method == 0 ? std::move(compressed) : inflateXml(compressed,record.size);
        if (crc32(0,reinterpret_cast<const Bytef*>(bytes.data()),static_cast<uInt>(bytes.size())) != record.crc
            ) fail("Corrupt ZIP member checksum");
        return bytes;
    }
    std::optional<std::string> xml(std::string_view name) {
        auto bytes=member(name);
        if(bytes&&(!validUtf8(*bytes)||bytes->find('\0')!=bytes->npos))fail("Corrupt ZIP XML or non-UTF-8 text");
        return bytes;
    }
    void copyAudio(const EmbeddedAudioCopy& copy, const std::stop_token& stop) {
        checkCancelled(stop);
        const auto key = pathKey(copy.member);
        const auto slash = key.find_last_of('/');
        const auto leaf = std::string_view(key).substr(slash == key.npos ? 0 : slash + 1);
        if (leaf.size() <= 4 || !leaf.ends_with(".wav")) fail("Embedded audio member must have a .wav extension");
        const auto it = entries.find(key);
        if (it == entries.end() || it->second.directory || it->second.name != copy.member)
            fail("Embedded WAV member not found by exact name");
        const auto& record = it->second;
        constexpr std::uint32_t chunkSize = 32 * 1024;
        std::array<Bytef, chunkSize> packed{}, decoded{};
        std::array<Bytef, 12> signature{};
        if (record.size < signature.size()) fail("Embedded audio is not a RIFF/WAVE file");
        input.seekg(static_cast<std::streamoff>(record.start));
        auto packedLeft = record.packed;
        auto readChunk = [&]() {
            const auto count = std::min(packedLeft, chunkSize);
            if (!input.read(reinterpret_cast<char*>(packed.data()), static_cast<std::streamsize>(count)))
                fail("Cannot read embedded WAV member");
            packedLeft -= count;
            return static_cast<uInt>(count);
        };

        // Only the caller's generated staging path is used, never a ZIP name.
        struct Output {
            const std::filesystem::path& path;
            HANDLE handle = INVALID_HANDLE_VALUE;
            bool keep = false;
            ~Output() {
                if (handle == INVALID_HANDLE_VALUE) return;
                CloseHandle(handle);
                if (!keep) DeleteFileW(path.c_str());
            }
            void write(const Bytef* bytes, uInt count) {
                if (count == 0) return;
                if (handle == INVALID_HANDLE_VALUE) {
                    handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (handle == INVALID_HANDLE_VALUE) fail("Cannot create staged WAV file");
                }
                DWORD written{};
                if (!WriteFile(handle, bytes, static_cast<DWORD>(count), &written, nullptr) || written != count)
                    fail("Cannot write staged WAV file");
            }
        } output{copy.destination};

        auto crc = crc32(0, Z_NULL, 0);
        std::uint32_t unpacked = 0;
        const auto emit = [&](const Bytef* bytes, uInt count) {
            if (count > record.size - unpacked) fail("Embedded WAV exceeds its declared size");
            crc = crc32(crc, bytes, count);
            const auto previous = unpacked;
            unpacked += count;
            // Do not create a file until a complete RIFF/WAVE signature has been seen.
            if (previous < signature.size()) {
                const auto prefix = std::min(count, static_cast<uInt>(signature.size() - previous));
                std::copy_n(bytes, prefix, signature.data() + previous);
                if (unpacked < signature.size()) return;
                const std::string_view header(reinterpret_cast<const char*>(signature.data()), signature.size());
                if (!header.starts_with("RIFF") || header.substr(8) != "WAVE") fail("Embedded audio is not a RIFF/WAVE file");
                output.write(signature.data(), static_cast<uInt>(signature.size()));
                bytes += prefix;
                count -= prefix;
            }
            output.write(bytes, count);
        };

        if (record.method == 0) {
            while (packedLeft != 0) {
                checkCancelled(stop);
                emit(packed.data(), readChunk());
            }
        } else {
            z_stream stream{};
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) fail("Cannot initialize ZIP inflater");
            struct Finish { z_stream& stream; ~Finish() { inflateEnd(&stream); } } finish{stream};
            for (;;) {
                checkCancelled(stop);
                if (stream.avail_in == 0 && packedLeft != 0) {
                    stream.avail_in = readChunk();
                    stream.next_in = packed.data();
                }
                const auto priorIn = stream.avail_in;
                stream.next_out = decoded.data();
                stream.avail_out = static_cast<uInt>(decoded.size());
                const auto status = inflate(&stream, Z_NO_FLUSH);
                const auto produced = static_cast<uInt>(decoded.size()) - stream.avail_out;
                if (status == Z_STREAM_END) {
                    if (stream.avail_in != 0 || packedLeft != 0) fail("Embedded WAV packed size mismatch");
                    emit(decoded.data(), produced);
                    break;
                }
                if (status != Z_OK || (priorIn == stream.avail_in && produced == 0))
                    fail("Malformed or truncated embedded WAV deflate stream");
                emit(decoded.data(), produced);
            }
        }
        checkCancelled(stop);
        if (unpacked != record.size || packedLeft != 0 || crc != record.crc)
            fail("Embedded WAV size or checksum mismatch");
        output.keep = true;
    }
};

struct Node {
    std::string tag, text;
    std::vector<std::pair<std::string,std::string>> attributes;
    std::vector<Node> children;
    std::string_view get(std::string_view key) const {
        for (const auto& [name,value] : attributes) if (name == key) return value;
        return {};
    }
    const Node* child(std::string_view tagName) const {
        for (const auto& node : children) if (node.tag == tagName) return &node;
        return nullptr;
    }
};

template<typename T> struct Release { void operator()(T* object) const { if (object) object->Release(); } };
std::string utf8(const wchar_t* text, UINT length) {
    if (length == 0) return {};
    return juce::String(juce::CharPointer_UTF16(text), length).toStdString();
}

Node parseXml(const std::string& bytes) {
    std::unique_ptr<IStream,Release<IStream>> stream(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size())));
    IXmlReader* raw{};
    if (!stream || FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&raw), nullptr))) fail("Cannot initialize XML reader");
    std::unique_ptr<IXmlReader,Release<IXmlReader>> reader(raw);
    if (FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit))
        || FAILED(reader->SetProperty(XmlReaderProperty_MaxElementDepth,64))
        || FAILED(reader->SetInput(stream.get()))) fail("Cannot configure safe XML input");
    Node root;
    std::vector<Node*> stack;
    std::size_t nodes = 0;
    XmlNodeType type{};
    HRESULT hr{};
    while ((hr = reader->Read(&type)) == S_OK) {
        if (type == XmlNodeType_Element) {
            if (++nodes > 100000 || stack.size() >= 64) fail("XML object or depth limit exceeded");
            const wchar_t* value{};
            UINT length{};
            if (FAILED(reader->GetQualifiedName(&value,&length)) || length > 256) fail("Invalid XML element name");
            auto tag = utf8(value,length);
            Node* node{};
            if (stack.empty()) {
                if (!root.tag.empty()) fail("Multiple XML roots");
                node = &root;
            } else node = &stack.back()->children.emplace_back();
            node->tag = std::move(tag);
            const bool empty = reader->IsEmptyElement() != FALSE;
            auto attr = reader->MoveToFirstAttribute();
            while (attr == S_OK) {
                if (node->attributes.size() >= 64 || FAILED(reader->GetQualifiedName(&value,&length)) || length > 256) fail("XML attribute limit exceeded");
                auto key = utf8(value,length);
                if (FAILED(reader->GetValue(&value,&length)) || length > 8192) fail("XML attribute value limit exceeded");
                node->attributes.emplace_back(std::move(key),utf8(value,length));
                attr = reader->MoveToNextAttribute();
            }
            if (FAILED(attr) || (!node->attributes.empty() && FAILED(reader->MoveToElement()))) fail("Invalid XML attributes");
            if (!empty) stack.push_back(node);
        } else if (type == XmlNodeType_EndElement) {
            if (stack.empty()) fail("Unexpected XML end tag");
            stack.pop_back();
        } else if ((type == XmlNodeType_Text || type == XmlNodeType_CDATA) && !stack.empty()) {
            const wchar_t* value{};
            UINT length{};
            if (FAILED(reader->GetValue(&value,&length)) || length > 65536) fail("XML text limit exceeded");
            auto& text = stack.back()->text;
            text += utf8(value,length);
            if (text.size() > 65536) fail("XML text limit exceeded");
        }
    }
    if (hr != S_FALSE || !stack.empty() || root.tag.empty()) fail("Malformed XML, prohibited DTD, or XML depth limit exceeded");
    return root;
}

std::optional<double> number(std::string_view s) {
    if (s.empty()) return {};
    if (s.front() == '+') s.remove_prefix(1);
    double value{};
    const auto last = s.data()+s.size();
    const auto parsed = std::from_chars(s.data(),last,value);
    if (parsed.ec != std::errc{} || parsed.ptr != last || !std::isfinite(value)) fail("Invalid DAWproject numeric value");
    return value;
}
TimeUnit unit(std::string_view value, TimeUnit fallback = {}) {
    if (value.empty()) return fallback;
    if (value == "beats") return TimeUnit::beats;
    if (value == "seconds") return TimeUnit::seconds;
    fail("Invalid DAWproject timeline unit");
}
bool flag(std::string_view s) {
    if (s == "true" || s == "1") return true;
    if (s.empty() || s == "false" || s == "0") return false;
    fail("Invalid DAWproject boolean value");
}
bool plugin(std::string_view tag) { return tag == "Vst3Plugin" || tag == "Vst2Plugin" || tag == "AuPlugin" || tag == "ClapPlugin"; }

struct Mapping {
    Archive& archive;
    ImportSessionDTO& dto;
    std::set<std::string> ids;
    std::vector<std::size_t> trackReferences;
    std::vector<std::pair<std::size_t,std::string_view>> clipReferences;
    bool studioPro01{};

    void warn(std::string message) {
        if (dto.warnings.size() < 200) dto.warnings.push_back(std::move(message));
        else if (dto.warnings.size() == 200) dto.warnings.emplace_back("Additional warnings omitted.");
    }
    void scanIds(const Node& n) {
        const auto id = n.get("id");
        if (!id.empty() && !ids.emplace(id).second) fail("Duplicate DAWproject source ID");
        for (const auto& child : n.children) scanIds(child);
    }
    void scanPaths(const Node& n) {
        if (n.tag == "File" || n.tag == "State") {
            auto path=n.get("path");
            if (flag(n.get("external"))) {
                if (n.tag == "State") fail("External plug-in state is not allowed");
                // External absolute local media is legal. Validate syntax only; no filesystem probes.
                std::string local(path);
                std::replace(local.begin(),local.end(),'\\','/');
                path=local;
                if (path.size()>=3 && ((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z')) && path.substr(1,2)==":/") path.remove_prefix(3);
                else if (path.starts_with('/')&&!path.starts_with("//")) path.remove_prefix(1);
                pathKey(path);
            } else pathKey(path);
        }
        for (const auto& child : n.children) scanPaths(child);
    }
    void mark(std::size_t i, Support support, std::string_view detail) {
        auto& obj = dto.objects[i];
        if (support > obj.support) obj.support = support;
        if (obj.detail.size() >= 512 || obj.detail.find(detail) != std::string::npos) return;
        if (!obj.detail.empty()) obj.detail += "; ";
        obj.detail += detail;
    }
    void asset(const Node& n, std::size_t index) {
        auto path = n.get("path");
        const bool state = n.tag == "State";
        using Location = AssetReference::Location;
        const auto location = flag(n.get("external")) ? Location::externalUnchecked : archive.contains(path) ? Location::embedded : Location::missing;
        dto.assets.push_back({index,std::string(path),state,location});
        if (location == Location::missing) mark(index,Support::partial,state ? "Embedded plug-in state missing" : "Embedded media missing");
        if (location == Location::externalUnchecked) mark(index,Support::partial,"External media was not opened or verified");
    }
    void walk(const Node& n, std::optional<std::size_t> parent = {}, TimeUnit inherited = {}, std::optional<std::size_t> track = {}, bool timeline = false, bool approvedContext = true, bool deviceList = false, TimeUnit warpContent = {}) {
        const auto tag = std::string_view(n.tag);
        if (tag == "File" || tag == "State") {
            if (!parent || dto.objects[*parent].kind != (tag == "File" ? Kind::media : Kind::device)) fail("DAWproject file reference has no valid owner");
            asset(n,*parent);
            return;
        }
        if (tag == "Arrangement" || tag == "Scenes") { timeline = true; inherited=TimeUnit::beats; }
        const bool container = tag == "Structure" || tag == "Arrangement" || tag == "Devices" || tag == "Sends" || tag == "Parameters";
        if (parent && dto.objects[*parent].kind == Kind::automation && tag == "Target") {
            for (const auto& [key,value] : n.attributes) dto.objects[*parent].properties.emplace_back("target."+key,value);
            return;
        }
        std::optional<Kind> kind;
        if (tag == "Track") kind=Kind::track;
        else if (tag == "Channel") kind=Kind::channel;
        else if (tag == "Send") kind=Kind::send;
        else if (deviceList) kind=Kind::device;
        else if (tag == "Transport") kind=Kind::transport;
        else if (tag == "Lanes" || tag == "Clips" || tag == "Notes" || tag == "Markers" || tag == "markers" || tag == "Warps" || tag == "ClipSlot" || tag == "Timeline") kind=Kind::lane;
        else if (tag == "Clip") kind=Kind::clip;
        else if (tag == "Note") kind=Kind::note;
        else if (tag == "Marker") kind=Kind::marker;
        else if (tag == "Audio" || tag == "Video") kind=Kind::media;
        else if (tag == "Points" || tag == "TempoAutomation" || tag == "TimeSignatureAutomation") kind=Kind::automation;
        else if (tag == "Warp") kind=Kind::warp;
        else if (tag == "Point" || tag.ends_with("Point")) kind=Kind::point;
        else if (tag == "Tempo" || tag == "TimeSignature" || tag == "Mute" || tag == "Pan" || tag == "Volume" || tag == "Enable" || tag == "Enabled" || tag.ends_with("Parameter")) kind=Kind::parameter;
        else if (tag == "Scenes" || tag == "Scene") {
            if (tag == "Scenes" && !n.children.empty()) warn("Clip launcher scenes are not supported in a Studio project preview");
            approvedContext = false;
        } else if (!container) {
            warn("Unmapped XML section: " + n.tag);
            if (parent) mark(*parent,Support::partial,"Unmapped child element");
            return;
        }
        const auto timeUnit = unit(n.get("timeUnit"),inherited);
        auto childUnit = timeUnit;
        if (kind) {
            Object obj;
            obj.kind=*kind; obj.parent=parent; obj.type=n.tag; obj.id=n.get("id"); obj.name=n.get("name"); obj.trackId=n.get("track");
            if (!obj.trackId.empty()) track=dto.objects.size();
            obj.trackScope=track;
            if (obj.name.empty() && *kind == Kind::device) obj.name=n.get("deviceName");
            obj.timeUnit=timeUnit; obj.contentTimeUnit=unit(n.get("contentTimeUnit"),tag == "Warp" ? warpContent : timeUnit);
            obj.time=number(n.get("time")); obj.duration=number(n.get("duration")); obj.properties=n.attributes;
            obj.durationUnit=*kind == Kind::media ? TimeUnit::seconds : timeUnit;
            if ((obj.kind == Kind::clip || obj.kind == Kind::note || obj.kind == Kind::marker || obj.kind == Kind::point || obj.kind == Kind::warp) && !obj.time) fail("DAWproject timeline event has no position");
            if ((obj.kind == Kind::note || obj.kind == Kind::media) && !obj.duration) fail("DAWproject event or media has no duration");
            if (obj.duration && *obj.duration < 0) fail("Negative DAWproject duration");
            if (obj.kind == Kind::clip || obj.kind == Kind::warp) for (const auto field : {"playStart", "playStop", "loopStart", "loopEnd", "contentTime", "fadeInTime", "fadeOutTime"}) number(n.get(field));
            const bool implicitChannel = studioPro01 && obj.kind == Kind::note
                && std::none_of(n.attributes.begin(),n.attributes.end(),[](const auto& attribute){return attribute.first=="channel";});
            if (obj.kind == Kind::note) {
                const auto inRange = [&](std::string_view key,double high) { auto val=number(n.get(key)); return val && *val >= 0 && *val <= high && std::trunc(*val)==*val; };
                if ((!implicitChannel && !inRange("channel",15)) || !inRange("key",127)) fail("Invalid DAWproject note channel or key");
                for (const auto key : {"vel", "rel"}) if (auto v=number(n.get(key)); v && (*v < 0 || *v > 1)) fail("Invalid DAWproject note velocity");
            }
            const auto index=dto.objects.size();
            dto.objects.push_back(std::move(obj));
            if (implicitChannel) mark(index,Support::partial,"Studio Pro 0.1 does not specify a MIDI channel for this note");
            if (!approvedContext || tag == "ClipSlot" || tag == "Video" || tag == "Timeline") mark(index,Support::unsupported,"No matching Studio arrangement feature");
            if (*kind == Kind::device) mark(index,tag == "Vst3Plugin" ? Support::partial : Support::unsupported,tag == "Vst3Plugin" ? "VST3 availability and state compatibility not verified" : "No supported device mapping");
            if (*kind == Kind::channel || *kind == Kind::send) mark(index,Support::partial,"Mixer mapping not verified in preview");
            if (tag == "Warps" || tag == "Warp" || tag == "Points" || (tag == "Clip" && (!n.get("reference").empty() || !n.get("loopEnd").empty()))) mark(index,Support::partial,"Timeline mapping may be incomplete");
            if (tag == "Track" && (n.get("contentType").find("video") != std::string_view::npos || n.get("contentType").find("automation") != std::string_view::npos)) mark(index,Support::partial,"Track contains features requiring separate mapping");
            if (!n.get("track").empty()) trackReferences.push_back(index);
            if (timeline && *kind == Kind::clip && !track) mark(index,Support::partial,"No arrangement track assigned");
            if (*kind == Kind::clip || tag == "Warps") childUnit=dto.objects[index].contentTimeUnit;
            if (*kind == Kind::media && std::count_if(n.children.begin(),n.children.end(),[](const Node& x){return x.tag=="File";}) != 1)
                fail("DAWproject media must have exactly one File");
            if (*kind == Kind::clip) {
                const auto reference=n.get("reference");
                if ((reference.empty() && n.children.size()!=1) || (!reference.empty() && !n.children.empty())) fail("DAWproject clip must have inline content or a reference");
                if (!reference.empty()) clipReferences.emplace_back(index,reference);
            }
            parent=index;
        }
        for (const auto& child : n.children) {
            const bool isWarp=tag == "Warps" && child.tag == "Warp";
            walk(child,parent,isWarp?timeUnit:childUnit,track,timeline,approvedContext && tag != "ClipSlot" && tag != "Timeline",tag == "Devices",isWarp?childUnit:TimeUnit{});
        }
    }
    void finish() {
        std::unordered_map<std::string_view,Kind> mapped;
        for (const auto& object : dto.objects) if (!object.id.empty()) mapped.emplace(object.id,object.kind);
        for (const auto i : trackReferences) {
            const auto found = mapped.find(dto.objects[i].trackId);
            if (found == mapped.end() || (found->second != Kind::track && found->second != Kind::channel)) mark(i,Support::partial,"Arrangement track reference does not resolve");
        }
        for (const auto& [i,ref] : clipReferences) {
            const auto target=mapped.find(ref);
            if (target==mapped.end() || (target->second!=Kind::lane && target->second!=Kind::media && target->second!=Kind::automation))
                fail("DAWproject clip content reference does not resolve to a timeline");
        }
        for (auto i = dto.objects.size();i>0;--i) {
            const auto& child = dto.objects[i-1];
            if (child.support != Support::supported && child.parent && dto.objects[*child.parent].support == Support::supported)
                mark(*child.parent,Support::partial,"Contains partially supported or unsupported data");
        }
    }
};
} // namespace

ImportSessionDTO DAWprojectReader::read(const std::filesystem::path& file) {
    Archive archive(file);
    const auto xml=archive.xml("project.xml");
    if (!xml) fail("DAWproject ZIP is missing project.xml");
    const auto root=parseXml(*xml);
    if (root.tag != "Project") fail("project.xml does not contain a DAWproject Project root");
    const auto* app=root.child("Application");
    if (!app || app->get("name").empty() || app->get("version").empty()) fail("DAWproject Application name or version missing");
    const auto version=root.get("version");
    // Studio Pro 8.2 exports a predecessor dialect with version 0.1.
    // Do not grant that exception to unrelated exporters or unknown versions.
    const bool studioPro01=version=="0.1" && app->get("name")=="Studio Pro";
    if (version!="1.0" && !studioPro01) {
        auto found=version.empty()?std::string("missing"):std::string(version.substr(0,40));
        for (auto& c:found) if (static_cast<unsigned char>(c)<32 || static_cast<unsigned char>(c)>126) c='?';
        throw std::runtime_error("Unsupported DAWproject version (found "+found+"; expected 1.0 or Studio Pro 0.1)");
    }
    ImportSessionDTO dto;
    dto.sourceFormat="DAWproject"; dto.sourceVersion=root.get("version"); dto.application=app->get("name"); dto.applicationVersion=app->get("version");
    dto.title=juce::String(file.stem().wstring().c_str()).toStdString();
    Mapping map{archive,dto,{}, {}, {}, studioPro01};
    if (studioPro01) map.warn("Studio Pro DAWproject 0.1: preview uses the legacy export layout; omitted MIDI channels are not inferred.");
    map.scanIds(root); map.scanPaths(root);
    if (const auto xmlMeta=archive.xml("metadata.xml")) {
        const auto metadata=parseXml(*xmlMeta);
        if (metadata.tag != "MetaData") fail("Invalid DAWproject metadata root");
        if (const auto* title=metadata.child("Title"); title && !title->text.empty()) dto.title=title->text;
        for (const auto& child : metadata.children) if (child.tag != "Title") map.warn("Metadata not mapped: " + child.tag);
    }
    std::set<std::string_view> singletons;
    for (const auto& child : root.children) {
        if ((child.tag == "Application" || child.tag == "Transport" || child.tag == "Structure" || child.tag == "Arrangement" || child.tag == "Scenes")
            && !singletons.emplace(child.tag).second) fail("Duplicate DAWproject top-level section");
        if (child.tag != "Application") map.walk(child);
    }
    map.finish();
    return dto;
}

namespace {
// Studio One format-9 musicx: observed unoptimized UBJSON containers,
// big-endian numeric scalars. Bound all reads, nesting and event counts.
// Never guess an unknown encoding or parse by scanning for pitch bytes.
class SongPerformance {
    std::string_view bytes;
    std::size_t at{}, values{};
    char peek() const {if(at>=bytes.size())fail("Truncated Song MIDI performance");return bytes[at];}
    char take(){const auto c=peek();++at;return c;}
    std::uint64_t bits(unsigned n){
        std::uint64_t v{};
        for(unsigned i=0;i<n;++i)v=(v<<8)|static_cast<unsigned char>(take());
        return v;
    }
    double scalar(char tag){
        double v{};
        switch(tag){
            case 'i':v=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(bits(1)));break;
            case 'U':v=static_cast<double>(bits(1));break;
            case 'I':v=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits(2)));break;
            case 'l':v=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits(4)));break;
            case 'L':v=static_cast<double>(std::bit_cast<std::int64_t>(bits(8)));break;
            case 'd':v=std::bit_cast<float>(static_cast<std::uint32_t>(bits(4)));break;
            case 'D':v=std::bit_cast<double>(bits(8));break;
            default:fail("Unsupported Song MIDI numeric encoding");
        }
        if(!std::isfinite(v))fail("Nonfinite Song MIDI number");
        return v;
    }
    static bool numericTag(char tag){return std::string_view("iUIlLdD").find(tag)!=std::string_view::npos;}
    std::string string(){
        const auto n=scalar(take());
        if(n<0||n>1048576||std::floor(n)!=n||n>static_cast<double>(bytes.size()-at))
            fail("Invalid Song MIDI string length");
        const auto count=static_cast<std::size_t>(n);
        std::string text(bytes.substr(at,count));at+=count;
        if(!validUtf8(text)||text.find('\0')!=text.npos)fail("Invalid Song MIDI string");
        return text;
    }
    void budget(unsigned depth){
        if(depth>32||++values>1000000)fail("Song MIDI performance complexity limit exceeded");
    }
    void skip(unsigned depth){
        budget(depth);const auto tag=take();
        if(numericTag(tag)){(void)scalar(tag);return;}
        if(tag=='S'){(void)string();return;}
        if(tag=='T'||tag=='F'||tag=='Z'||tag=='N')return;
        if(tag=='C'){(void)take();return;}
        if(tag=='{'||tag=='['){
            const auto end=tag=='{'?'}':']';
            while(peek()!=end){if(tag=='{')(void)string();skip(depth+1);}
            (void)take();return;
        }
        fail("Unsupported Song MIDI performance encoding");
    }
public:
    struct Note {std::map<std::string,double> fields;};
    std::vector<Note> notes;
    bool unsupportedEvents{},envelopes{},variations{},extraFields{},quantize{};
    explicit SongPerformance(std::string_view source):bytes(source){
        if(take()!='{')fail("Invalid Song MIDI performance root");
        bool format=false,events=false;std::set<std::string> keys;
        while(peek()!='}'){
            budget(0);auto key=string();
            if(!keys.insert(key).second)fail("Duplicate Song MIDI performance field");
            if(key=="timeFormat"){
                if(scalar(take())!=2)fail("Unsupported Song MIDI performance time format");
                format=true;
            }else if(key=="events"){
                if(take()!='[')fail("Invalid Song MIDI event list");
                events=true;
                while(peek()!=']'){
                    budget(1);
                    if(take()!='{')fail("Invalid Song MIDI event");
                    Note note;std::set<std::string> fields;
                    while(peek()!='}'){
                        budget(2);auto field=string();
                        if(!fields.insert(field).second)fail("Duplicate Song MIDI event field");
                        if(field.starts_with("quantize."))quantize=true;
                        if(field=="soundVariationId")variations=true;
                        const bool known=field=="start"||field=="length"||field=="pitch"||field=="velocity"||field=="channel"||field=="noteId"||field.starts_with("quantize.")||field=="soundVariationId";
                        if(!known)extraFields=true;
                        if(numericTag(peek()))note.fields.emplace(std::move(field),scalar(take()));
                        else{extraFields=true;skip(3);}
                    }
                    (void)take();
                    if(!note.fields.contains("pitch")||!note.fields.contains("length")||!note.fields.contains("velocity")){
                        unsupportedEvents=true;continue;
                    }
                    if(notes.size()>=32768)fail("Song MIDI note limit exceeded");
                    notes.push_back(std::move(note));
                }
                (void)take();
            }else if(key=="envelopes"){
                if(peek()!='[')fail("Invalid Song MIDI envelopes");
                (void)take();envelopes=peek()!=']';
                while(peek()!=']')skip(1);
                (void)take();
            }else{extraFields=true;skip(1);}
        }
        (void)take();
        if(!format||!events||at!=bytes.size())fail("Incomplete or trailing Song MIDI performance");
    }
};
std::string songClassId(std::string_view value){
    std::string id;
    for(const char ch:value){
        if(ch=='{'||ch=='}'||ch=='-')continue;
        if(ch>='a'&&ch<='f')id+=static_cast<char>(ch-'a'+'A');
        else if((ch>='0'&&ch<='9')||(ch>='A'&&ch<='F'))id+=ch;
        else fail("Invalid Song VST3 class ID");
    }
    if(id.size()!=32)fail("Invalid Song VST3 class ID length");
    return id;
}
void decodeSongPreset(SongVst3& pluginState){
    const std::string_view bytes=pluginState.preset;
    const auto u64=[&](std::size_t offset){
        return std::uint64_t(le(bytes,offset,4))|(std::uint64_t(le(bytes,offset+4,4))<<32);
    };
    if(bytes.size()<56||bytes.substr(0,4)!="VST3"||le(bytes,4,4)!=1||
       songClassId(bytes.substr(8,32))!=pluginState.classId)
        fail("Song VST3 preset header/class ID mismatch");
    const auto list=u64(40);
    if(list<48||list>bytes.size()-8||bytes.substr(static_cast<std::size_t>(list),4)!="List")
        fail("Invalid Song VST3 chunk list");
    const auto count=le(bytes,static_cast<std::size_t>(list)+4,4);
    if(count>128||list+8+std::uint64_t(count)*20!=bytes.size())fail("Invalid Song VST3 chunk count");
    bool comp=false,cont=false;
    std::vector<std::pair<std::uint64_t,std::uint64_t>> ranges;
    for(std::uint32_t n=0;n<count;++n){
        const auto at=static_cast<std::size_t>(list+8+std::uint64_t(n)*20);
        const auto tag=bytes.substr(at,4);
        const auto offset=u64(at+4),size=u64(at+12);
        if(offset<48||offset>list||size>list-offset)fail("Invalid Song VST3 chunk bounds");
        if(size)ranges.emplace_back(offset,offset+size);
        if(tag=="Comp"||tag=="Cont"){
            auto& seen=tag=="Comp"?comp:cont;
            if(seen||size>16*MiB)fail("Duplicate or oversized Song VST3 state");
            seen=true;
            (tag=="Comp"?pluginState.component:pluginState.controller)=bytes.substr(static_cast<std::size_t>(offset),static_cast<std::size_t>(size));
        }
    }
    std::sort(ranges.begin(),ranges.end());
    for(std::size_t i=1;i<ranges.size();++i)if(ranges[i].first<ranges[i-1].second)fail("Overlapping Song VST3 chunks");
    if(!comp)fail("Song VST3 preset missing component state");
}
std::string exactNumber(double value){
    std::array<char,64> chars{};
    const auto result=std::to_chars(chars.data(),chars.data()+chars.size(),value,std::chars_format::general,std::numeric_limits<double>::max_digits10);
    if(result.ec!=std::errc{})fail("Cannot retain Song MIDI number");
    return {chars.data(),result.ptr};
}

} // namespace

ImportSessionDTO StudioOneSongReader::read(const std::filesystem::path& file) {
    Archive archive(file);
    const auto xml = archive.xml("Song/song.xml");
    if (!xml) fail("Studio One archive is missing Song/song.xml");
    const auto root = parseXml(::ui::live::normalizeStudioOneXml(*xml));
    if (root.tag != "Song") fail("Unsupported Studio One Song XML root");

    ImportSessionDTO dto;
    dto.sourceFormat = "StudioOneSong";
    dto.title = juce::String(file.stem().wstring().c_str()).toStdString();
    if (const auto metadata = archive.xml("metainfo.xml")) {
        const auto meta = parseXml(*metadata);
        if (meta.tag != "MetaInformation") fail("Invalid Studio One metadata");
        for (const auto& item : meta.children) {
            if (item.tag != "Attribute") continue;
            if (item.get("id") == "Document:FormatVersion") dto.sourceVersion = item.get("value");
            if (item.get("id") == "Document:Title" && !item.get("value").empty()) dto.title = item.get("value");
            if (item.get("id") == "Document:Generator") {
                const auto generator = item.get("value");
                const auto slash = generator.find('/');
                dto.application = generator.substr(0, slash);
                if (slash != generator.npos) dto.applicationVersion = generator.substr(slash + 1);
            }
        }
    }
    if (dto.sourceVersion != "9") fail("Studio One Song reader currently requires format version 9");
    const auto childById = [](const Node& parent, std::string_view tag, std::string_view id) -> const Node* {
        for (const auto& child : parent.children)
            if (child.tag == tag && child.get("x_id") == id) return &child;
        return nullptr;
    };
    const auto* model = childById(root, "Attributes", "Root");
    if (!model) fail("Studio One Song model is missing");
    const auto* context = childById(*model, "Attributes", "timeContext");
    const auto* tracks = childById(*model, "List", "Tracks");
    if (!context || !tracks) fail("Studio One Song time context or tracks are missing");

    auto warning = [&](std::string message) {
        if (dto.warnings.size() < 200) dto.warnings.push_back(std::move(message));
        else if (dto.warnings.size() == 200) dto.warnings.emplace_back("Additional Song warnings omitted.");
    };
    auto add = [&](Kind kind, const Node& source, std::optional<std::size_t> parent = {}) -> std::size_t {
        Object object{};
        object.kind = kind;
        object.type = source.tag;
        object.parent = parent;
        object.name = source.get("name");
        object.properties = source.attributes;
        object.support = Support::partial;
        object.detail = "Source metadata retained; see compatibility warnings";
        dto.objects.push_back(std::move(object));
        return dto.objects.size() - 1;
    };
    const auto contextIndex = add(Kind::transport, *context);
    for (const auto& lane : context->children) {
        if (lane.tag != "TempoMap" && lane.tag != "TimeSignatureMap") continue;
        const bool tempo = lane.tag == "TempoMap";
        const auto laneIndex = add(Kind::lane, lane, contextIndex);
        for (const auto& segment : lane.children) {
            if (segment.tag != (tempo ? "TempoMapSegment" : "TimeSignatureMapSegment")) continue;
            auto& row = dto.objects[add(Kind::point, segment, laneIndex)];
            const auto start = number(segment.get("start"));
            if (!start || *start < 0) fail("Invalid Studio One map segment start");
            // Only the origin of the map is verified across the supplied schema.
            // Preserve nonzero segment positions verbatim until their units are verified.
            row.time = start;
            row.timeUnit = *start == 0 ? TimeUnit::beats : TimeUnit::unspecified;
            if (*start != 0) warning("Nonzero Song map segment retained with unverified time unit.");
            if (tempo) {
                const auto period = number(segment.get("tempo"));
                if (!period || *period <= 0 || !std::isfinite(60.0 / *period))
                    fail("Invalid Studio One tempo period");
                row.properties.emplace_back("bpm", std::to_string(60.0 / *period));
                if (segment.get("curveType") != "0")
                    warning("Song tempo curve retained; ramp interpretation not verified.");
            } else {
                const auto n = number(segment.get("numerator"));
                const auto d = number(segment.get("denominator"));
                if (!n || !d || *n < 1 || *n > 64 || *d < 1 || *d > 64 ||
                    std::floor(*n) != *n || std::floor(*d) != *d ||
                    (static_cast<int>(*d) & (static_cast<int>(*d) - 1)) != 0)
                    fail("Invalid Studio One time signature");
            }
        }
    }
    for (const auto& lane : tracks->children) {
        Kind kind{};
        const char* eventTag = nullptr;
        if (lane.tag == "ArrangerTrack") { kind = Kind::section; eventTag = "ArrangerEvent"; }
        else if (lane.tag == "ChordTrack") { kind = Kind::chord; eventTag = "ChordEvent"; }
        else if (lane.tag == "MarkerTrack") { kind = Kind::marker; eventTag = "MarkerEvent"; }
        else continue;
        const auto laneIndex = add(Kind::lane, lane);
        for (const auto& event : lane.children) {
            if (event.tag != eventTag) continue;
            auto& row = dto.objects[add(kind, event, laneIndex)];
            row.time = number(event.get("start"));
            if (!row.time || *row.time < 0) fail("Invalid Studio One event start");
            const auto format = event.get("timeFormat");
            // The supplied format-9 project uses 2 for quarter-note positions.
            // Other time formats are preserved but never guessed.
            if (format == "2") row.timeUnit = TimeUnit::beats;
            else warning("Song event time format is not verified; source position retained.");
            if (kind != Kind::marker) {
                row.duration = number(event.get("length"));
                if (!row.duration || *row.duration < 0) fail("Invalid Studio One event length");
                row.durationUnit = row.timeUnit;
            }
            // Preserve source order, offsets, colors and events beyond End.
            if (kind == Kind::chord) {
                const auto* encoded = childById(event, "Attributes", "chord");
                if (!encoded) {
                    warning("Song chord has no encoded chord data.");
                    continue;
                }
                juce::XmlElement chord("Attributes");
                for (const auto& [key, value] : encoded->attributes) {
                    if (key == "x_id") continue;
                    row.properties.emplace_back("chord." + key, value);
                    chord.setAttribute(juce::String::fromUTF8(key.c_str()), juce::String::fromUTF8(value.c_str()));
                }
                try { row.name = ::ui::live::decodeStudioOneChord(chord).toStdString(); }
                catch (const std::exception&) {
                    warning("Song chord symbol could not be decoded; raw chord fields retained.");
                }
            }
        }
    }
    // Audio source adapter: only direct active Audio MediaTracks/Events.
    // Resolve identities through the media pool, never by matching display names.
    std::optional<Node> pool;
    if(const auto data=archive.xml("Song/mediapool.xml"))
        pool=parseXml(::ui::live::normalizeStudioOneXml(*data));
    std::map<std::string,const Node*> sources;
    if(pool){
        if(pool->tag!="MediaPool")fail("Invalid Song media pool");
        const auto scan=[&](const auto& self,const Node& node)->void{
            if(node.tag=="AudioClip"||node.tag=="MusicClip"){
                const auto id=node.get("mediaID");
                if(id.empty()||!sources.emplace(std::string(id),&node).second)fail("Invalid or duplicate Song media ID");
            }
            for(const auto& child:node.children)self(self,child);
        };
        scan(scan,*pool);
    }
    std::optional<Node> mixer;
    if(const auto data=archive.xml("Devices/audiomixer.xml"))
        mixer=parseXml(::ui::live::normalizeStudioOneXml(*data));
    std::map<std::string,const Node*> channels;
    if(mixer){
        if(mixer->tag!="AudioMixer")fail("Invalid Song mixer");
        const auto scan=[&](const auto& self,const Node& node)->void{
            if(node.tag=="AudioTrackChannel"||node.tag=="AudioSynthChannel"||node.tag=="AudioOutputChannel"){
                const auto* uid=childById(node,"UID","uniqueID");
                if(uid&&!uid->get("uid").empty()&&!channels.emplace(std::string(uid->get("uid")),&node).second)
                    fail("Duplicate Song mixer channel ID");
            }
            for(const auto& child:node.children)self(self,child);
        };
        scan(scan,*mixer);
    }
    const auto uidOf=[&](const Node& node,std::string_view key){
        const auto* uid=childById(node,"UID",key);
        return uid?std::string(uid->get("uid")):std::string{};
    };
    const auto connection=[&](const Node& node,std::string_view key,std::string_view suffix){
        const auto* edge=childById(node,"Connection",key);
        if(!edge)return std::string{};
        const auto id=edge->get("objectID");
        if(!id.ends_with(suffix)||id.size()<=suffix.size())return std::string{};
        return std::string(id.substr(0,id.size()-suffix.size()));
    };
    std::optional<Node> musicDevice,synthFolder;
    if(const auto data=archive.xml("Devices/musictrackdevice.xml")){
        musicDevice=parseXml(::ui::live::normalizeStudioOneXml(*data));
        if(musicDevice->tag!="MusicTrackDevice")fail("Invalid Song music device");
    }
    if(const auto data=archive.xml("Devices/audiosynthfolder.xml")){
        synthFolder=parseXml(::ui::live::normalizeStudioOneXml(*data));
        if(synthFolder->tag!="AudioSynthFolder")fail("Invalid Song synth folder");
    }
    std::map<std::string,const Node*> musicChannels,synths;
    if(musicDevice){
        const auto scan=[&](const auto& self,const Node& node)->void{
            if(node.tag=="MusicTrackChannel"){
                auto uid=uidOf(node,"uniqueID");
                if(!uid.empty()&&!musicChannels.emplace(std::move(uid),&node).second)fail("Duplicate Song MIDI channel");
            }
            for(const auto& child:node.children)self(self,child);
        };scan(scan,*musicDevice);
    }
    if(synthFolder)for(const auto& synth:synthFolder->children){
        if(const auto* data=childById(synth,"Attributes","deviceData")){
            auto uid=uidOf(*data,"uniqueID");
            if(!uid.empty()&&!synths.emplace(std::move(uid),&synth).second)fail("Duplicate Song synth identity");
        }
    }
    const Node* main=nullptr;std::string mainId;
    unsigned outputCount{};
    for(const auto& [id,channel]:channels)if(channel->tag=="AudioOutputChannel"){
        ++outputCount;main=channel;mainId=id;
    }
    if(outputCount!=1){main=nullptr;mainId.clear();if(outputCount>1)warning("Multiple Song output channels not mapped to hardware.");}
    std::uint64_t presetBytes{};
    const auto addPlugin=[&](const Node& device,std::optional<std::size_t> track,bool instrument){
        const auto* ghost=childById(device,"Attributes","ghostData");
        const auto* info=ghost?childById(*ghost,"Attributes","classInfo"):nullptr;
        const auto* path=childById(device,"String","presetPath");
        if(!info||!path||ghost->get("presetType")!="vstpreset"||!info->get("subCategory").starts_with("VST3")){
            warning("Unsupported Song device omitted: "+std::string(device.get("name")));return;
        }
        SongVst3 state;state.track=track;state.instrument=instrument;state.bypass=device.get("bypass")=="1";
        state.name=info->get("name");state.classId=songClassId(info->get("classID"));
        if(songClassId(uidOf(device,"deviceClassID"))!=state.classId)
            fail("Song VST3 device identity mismatch");
        state.member=path->get("text");
        if(!state.member.starts_with("Presets/")||!state.member.ends_with(".vstpreset"))fail("Unsafe Song VST3 preset reference");
        const auto bytes=archive.member(state.member,33*MiB);
        if(!bytes)fail("Song VST3 preset missing");
        if(bytes->size()>64*MiB-presetBytes)fail("Song VST3 preset aggregate budget exceeded");
        presetBytes+=bytes->size();state.preset=*bytes;decodeSongPreset(state);
        if(dto.songPlugins.size()>=32)fail("Song VST3 count exceeds native project limit");
        state.object=add(Kind::device,device,track);
        auto& row=dto.objects[state.object];row.type="Vst3Plugin";row.name=state.name;
        row.properties.emplace_back("classId",state.classId);
        row.properties.emplace_back("presetPath",state.member);
        row.properties.emplace_back("role",instrument?"instrument":"effect");
        row.detail="Verified VST3 preset; installation and restore checked on target PC";
        dto.songPlugins.push_back(std::move(state));
    };
    const auto addInserts=[&](const Node& channel,std::optional<std::size_t> track){
        if(const auto* inserts=childById(channel,"Attributes","Inserts"))
            for(const auto& item:inserts->children)
                if(item.tag=="Attributes"&&!uidOf(item,"deviceClassID").empty())addPlugin(item,track,false);
        if(const auto* post=childById(channel,"Attributes","PostFaderInserts"))
            for(const auto& item:post->children)if(!uidOf(item,"deviceClassID").empty())
                warning("Song post-fader insert not imported: "+std::string(channel.get("label")));
        if(const auto* sends=childById(channel,"Attributes","Sends"))
            for(const auto& item:sends->children)if(item.get("x_id")!="Presets")
                warning("Song send not imported: "+std::string(channel.get("label")));
    };
    if(main){
        const auto index=add(Kind::channel,*main);
        dto.objects[index].detail="Main gain and pre-fader inserts";
        if(main->get("mute")=="1")warning("Song Main mute not supported.");
        addInserts(*main,{});
    }
    std::set<std::string> usedSynths,usedChannels;
    std::size_t midiNotes{};
    std::map<std::string,SongPerformance> performances;
    for(const auto& track:tracks->children){
        if(track.tag!="MediaTrack")continue;
        const bool music=track.get("mediaType")=="Music";
        if(!music&&track.get("mediaType")!="Audio")continue;
        const auto trackIndex=add(Kind::track,track);
        auto& target=dto.objects[trackIndex];
        target.id=track.get("trackID");
        target.detail=music?"Instrument track and verified single-output routing":"Audio track and static mixer settings";
        target.support=Support::supported;
        if(music){
            if(const auto* settings=childById(track,"Attributes","attributes"))
                if(settings->get("drumMode")=="1")target.properties.emplace_back("drumMode","1");
        }
        const auto channelId=uidOf(track,"channelID");
        const Node* audioChannel=nullptr;const Node* instrument=nullptr;
        if(music){
            const auto found=musicChannels.find(channelId);
            if(found!=musicChannels.end()){
                const auto& midiChannel=*found->second;
                const auto outId=connection(midiChannel,"instrumentOut","/Input");
                const auto synthId=connection(midiChannel,"destination","/MusicInput00");
                const auto out=channels.find(outId),synth=synths.find(synthId);
                if(out!=channels.end()&&out->second->tag=="AudioSynthChannel"&&out->second->get("synthPort")=="0"&&synth!=synths.end()){
                    const auto* ports=childById(*synth->second,"List","synthChannels");
                    if(ports&&ports->children.size()==1&&ports->children.front().tag=="UID"&&ports->children.front().get("uid")==outId&&
                       !usedSynths.contains(synthId)&&!usedChannels.contains(outId)){
                        usedSynths.insert(synthId);usedChannels.insert(outId);
                        audioChannel=out->second;instrument=synth->second;
                    }
                }
                if(!instrument)warning("Song instrument route not representable (shared/multi-output or unsupported MIDI port): "+target.name);
                if(midiChannel.get("mute")=="1")target.properties.emplace_back("midi.mute","1");
                if(midiChannel.get("solo")=="1")target.properties.emplace_back("midi.solo","1");
            }else if(!channelId.empty())warning("Song MIDI mixer channel not found: "+target.name);
        }else if(const auto found=channels.find(channelId);found!=channels.end()&&found->second->tag=="AudioTrackChannel"){
            audioChannel=found->second;
        }else if(!channelId.empty())warning("Song track mixer channel not found: "+target.name);
        if(audioChannel){
            for(const auto key:{"gain","pan","mute","solo"})
                if(!audioChannel->get(key).empty())target.properties.emplace_back(std::string("mix.")+key,audioChannel->get(key));
            if(const auto* destination=childById(*audioChannel,"Connection","destination")){
                target.properties.emplace_back("destination",std::string(destination->get("objectID")));
                if(mainId.empty()||connection(*audioChannel,"destination","/Input")!=mainId)
                    warning("Song output route not representable; using Main: "+target.name);
            }
        }
        if(instrument)addPlugin(*instrument,trackIndex,true);
        if(audioChannel)addInserts(*audioChannel,trackIndex);
        const auto* events=childById(track,"List","Events");
        if(!events)continue;
        for(const auto& event:events->children){
            if(music){
                if(event.tag!="MusicPart"){warning("Unsupported Song MIDI event type: "+event.tag);continue;}
                const auto clipIndex=add(Kind::clip,event,trackIndex);
                auto& part=dto.objects[clipIndex];
                part.time=number(event.get("start"));part.duration=number(event.get("length"));
                if(!part.time||*part.time<0||!part.duration||*part.duration<=0||event.get("timeFormat")!="2")
                    fail("Invalid or unsupported Song MIDI part range");
                part.timeUnit=part.durationUnit=part.contentTimeUnit=TimeUnit::beats;
                part.support=Support::supported;part.detail="MIDI notes and source window";
                const auto found=sources.find(std::string(event.get("clipID")));
                if(found==sources.end()||found->second->tag!="MusicClip")fail("Song MIDI part has no MusicClip media-pool entry");
                const auto* url=childById(*found->second,"Url","dataPath");
                if(!url||!url->get("url").starts_with("media:///"))fail("Unsupported Song MIDI performance reference");
                const auto memberName=std::string(url->get("url").substr(9));
                if(!memberName.starts_with("Performances/")||!memberName.ends_with(".musicx"))
                    fail("Unsupported Song MIDI performance path");
                part.properties.emplace_back("performancePath",memberName);
                part.properties.emplace_back("mediaID",std::string(event.get("clipID")));
                auto cached=performances.find(memberName);
                if(cached==performances.end()){
                    const auto data=archive.member(memberName);
                    if(!data)fail("Song MIDI performance member missing");
                    cached=performances.try_emplace(memberName,*data).first;
                }
                const auto& decoded=cached->second;
                if(decoded.notes.size()>32768-midiNotes)fail("Song project MIDI note limit exceeded");
                midiNotes+=decoded.notes.size();
                if(decoded.unsupportedEvents||decoded.envelopes||decoded.extraFields)
                    warning("Song MIDI controllers/expressions or unknown event fields not interpreted: "+part.name);
                if(decoded.variations)warning("Song sound variation assignments retained as metadata only: "+part.name);
                if(decoded.quantize)warning("Song quantize history retained as metadata; imported note values are the stored start/length/velocity: "+part.name);
                for(const auto& note:decoded.notes){
                    Object row{};row.kind=Kind::note;row.type="MusicNote";row.parent=clipIndex;
                    row.timeUnit=row.durationUnit=TimeUnit::beats;
                    row.time=note.fields.contains("start")?note.fields.at("start"):0.;
                    row.duration=note.fields.at("length");
                    row.support=Support::supported;row.detail="MIDI note";
                    for(const auto& [key,value]:note.fields)row.properties.emplace_back(key,exactNumber(value));
                    dto.objects.push_back(std::move(row));
                }
                continue;
            }
            if(event.tag!="AudioEvent"){warning("Unsupported Song audio event type: "+event.tag);continue;}
            const auto clipIndex=add(Kind::clip,event,trackIndex);
            auto& clip=dto.objects[clipIndex];
            clip.time=number(event.get("start"));
            clip.duration=number(event.get("length"));
            if(!clip.time||*clip.time<0||!clip.duration||*clip.duration<=0)fail("Invalid Song audio range");
            // Cross-checked against Capture: start is in quarter notes for both
            // observed formats; length/offset are seconds (0) or quarter notes (2).
            clip.timeUnit=TimeUnit::beats;
            const auto format=event.get("timeFormat");
            if(format=="0")clip.durationUnit=clip.contentTimeUnit=TimeUnit::seconds;
            else if(format=="2")clip.durationUnit=clip.contentTimeUnit=TimeUnit::beats;
            else fail("Unverified Song audio event time format");
            clip.detail="Audio placement; source WAV must be available";
            if(const auto* volume=childById(event,"Attributes","volumeCurve")){
                for(const auto& [key,value]:volume->attributes)
                    clip.properties.emplace_back("volumeCurve."+key,value);
                warning("Song clip gain/envelope not applied (raw level retained): "+clip.name);
            }
            const auto found=sources.find(std::string(event.get("clipID")));
            if(found==sources.end()||found->second->tag!="AudioClip")fail("Song audio clip has no media-pool entry");
            const auto* url=childById(*found->second,"Url","path");
            if(!url||url->get("url").empty())fail("Song audio source path missing");
            const auto sourceUrl=std::string(url->get("url"));
            clip.properties.emplace_back("sourceUrl",sourceUrl);
            if(const auto* info=childById(*found->second,"Attributes","format")){
                for(const auto key:{"sampleRate","frameCount","bitDepth","sampleType"})
                    if(!info->get(key).empty())clip.properties.emplace_back(key,info->get(key));
                if(!info->get("numChannels").empty())clip.properties.emplace_back("channels",info->get("numChannels"));
            }
            // Never open archived absolute paths automatically. Recognize the
            // original project-relative Media/Bounces leaf; all other URLs need
            // an explicit user-selected replacement in the existing relink UI.
            std::string reference=sourceUrl;
            auto location=AssetReference::Location::missing;
            if(sourceUrl.starts_with("file:///")){
                const auto last=sourceUrl.rfind('/');
                const auto previous=last==sourceUrl.npos?sourceUrl.npos:sourceUrl.rfind('/',last-1);
                const auto prior=previous==sourceUrl.npos?sourceUrl.npos:sourceUrl.rfind('/',previous-1);
                if(prior!=sourceUrl.npos&&previous>prior&&last>previous){
                    const auto folder=sourceUrl.substr(previous+1,last-previous-1);
                    const auto project=sourceUrl.substr(prior+1,previous-prior-1);
                    if((folder=="Media"||folder=="Bounces")&&
                       juce::String::fromUTF8(project.c_str()).equalsIgnoreCase(juce::String(file.stem().wstring().c_str()))){
                        const auto relative=sourceUrl.substr(previous+1);
                        try{(void)pathKey(relative);reference=relative;location=AssetReference::Location::externalUnchecked;}
                        catch(const std::exception&){/* Explicit relink for unsafe or unfamiliar names. */}
                    }
                }
            }
            dto.assets.push_back({clipIndex,std::move(reference),false,location});
        }
    }
    if (const auto transport = archive.xml("Devices/transportdevice.xml")) {
        const auto device = parseXml(*transport);
        if (device.tag != "TransportDevice") fail("Invalid Studio One transport device");
        const auto index = add(Kind::transport, device);
        auto& row = dto.objects[index];
        row.time = number(device.get("position"));
        row.timeUnit = TimeUnit::seconds;
        // Loop coordinates and active state remain raw source properties.
    }
    warning("Song import supports VST3 single-output instruments, pre-fader inserts and Main routing. Other routing, sends, clip gain curves, controllers, Sound Variations and automation are not restored.");
    warning("Source playhead and loop are retained as metadata, not applied to the running transport.");
    return dto;
}

void DAWprojectReader::copyEmbeddedAudio(const std::filesystem::path& file, const std::vector<EmbeddedAudioCopy>& copies, std::stop_token stop) {
    checkCancelled(stop);
    if (copies.empty()) return;
    Archive archive(file, stop);
    for (const auto& copy : copies) archive.copyAudio(copy, stop);
    checkCancelled(stop);
}

ImportSessionDTO MRSProjectImport::read(const std::filesystem::path& file) {
    if (juce::String(file.extension().wstring().c_str()).equalsIgnoreCase(".song"))
        return StudioOneSongReader::read(file);
    return DAWprojectReader::read(file);
}

Preview MRSProjectImport::preview(const std::filesystem::path& file) {
    Preview result;
    try {
        result.session=read(file);
        result.projects=1;
        for (const auto& item : result.session->objects) {
            if (item.kind == Kind::track) ++result.tracks;
            if (item.kind == Kind::clip) ++result.clips;
            if (item.kind == Kind::device) { ++result.devices; if (plugin(item.type)) ++result.plugins; }
        }
    } catch (const std::exception& e) { result.error=e.what(); }
    return result;
}
} // namespace mrs::project_import
