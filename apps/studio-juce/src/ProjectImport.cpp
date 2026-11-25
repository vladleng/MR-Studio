#include "ProjectImport.h"
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
    std::optional<std::string> xml(std::string_view name) {
        const auto it = entries.find(std::string(name));
        if (it == entries.end()) return {};
        const auto& record = it->second;
        if (record.name != name || record.directory || record.size == 0 || record.size > maxXml || record.packed > maxXml + 65536
            || record.packed == 0 || record.size > std::uint64_t(record.packed) * 512 + 65536)
            fail("Invalid ZIP XML member or XML expansion limit exceeded");
        auto compressed = at(record.start, record.packed);
        auto bytes = record.method == 0 ? std::move(compressed) : inflateXml(compressed,record.size);
        if (crc32(0,reinterpret_cast<const Bytef*>(bytes.data()),static_cast<uInt>(bytes.size())) != record.crc
            || !validUtf8(bytes) || bytes.find('\0') != bytes.npos) fail("Corrupt ZIP XML or non-UTF-8 text");
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

void DAWprojectReader::copyEmbeddedAudio(const std::filesystem::path& file, const std::vector<EmbeddedAudioCopy>& copies, std::stop_token stop) {
    checkCancelled(stop);
    if (copies.empty()) return;
    Archive archive(file, stop);
    for (const auto& copy : copies) archive.copyAudio(copy, stop);
    checkCancelled(stop);
}

Preview MRSProjectImport::preview(const std::filesystem::path& file) {
    Preview result;
    try {
        result.session=DAWprojectReader::read(file);
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
