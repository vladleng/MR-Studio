#pragma once
#include "../apps/studio-juce/src/LiveStudioOneShowReader.h"

namespace ui::live::test {
inline void studioOneShowReader() {
    const std::string fixture = R"SHOW(<?xml version="1.0" encoding="UTF-8"?>
<Show>
  <Attributes x:id="Model">
    <Attributes x:id="setList">
      <List x:id="Items">
        <SetListItem tempo="0.5" length="16" timeSignature.numerator="4" timeSignature.denominator="4">
          <PersistentAttributes x:id="metaInfo">
            <Attribute id="Media:Title" value="Same name"/>
          </PersistentAttributes>
          <List x:id="sections">
            <SetListItemSection name="Intro" timeFormat="0" localOffset="0" length="8"/>
          </List>
          <List x:id="chords">
            <SetListItemChord timeFormat="0" localOffset="2" length="2">
              <Attributes x:id="chord" root="0" intervals="1 0 0 0 3 0 0 5 0 0 7 0" type="0"/>
            </SetListItemChord>
          </List>
        </SetListItem>
        <SetListItem tempo="0.75" length="12" timeSignature.numerator="4" timeSignature.denominator="4">
          <PersistentAttributes x:id="metaInfo">
            <Attribute id="Media:Title" value="Same name"/>
          </PersistentAttributes>
          <List x:id="sections">
            <SetListItemSection name="Bad short event" timeFormat="0" localOffset="0" length="0.1"/>
          </List>
        </SetListItem>
      </List>
      <MixerSnapshot::ChannelState/>
    </Attributes>
  </Attributes>
</Show>)SHOW";
    const auto fail = [](const char* reason) { throw std::runtime_error(reason); };
    const auto normalization = normalizeStudioOneXml(
        "<Show><Attributes x:id=\"Model\" title=\"keep x:id=inside value :: here\"/></Show>");
    if (normalization.find("x_id=\"Model\"") == std::string::npos ||
        normalization.find("keep x:id=inside value :: here") == std::string::npos)
        fail("Studio One XML compatibility transform modified an attribute value");
    std::string tooDeep = "<Show>";
    for (int i=0;i<65;++i) tooDeep += "<N>";
    for (int i=0;i<65;++i) tooDeep += "</N>";
    tooDeep += "</Show>";
    try {
        (void)readStudioOneShowXml(tooDeep);
        fail("Excessive Studio One XML nesting must be rejected");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("nesting limit")) throw;
    }
    // Studio Pro hex degree labels B (= 11th) / D (= 13th) are decoded by
    // the accepted Smart Improviser chord model, not interpreted as decimals.
    {
        juce::XmlDocument xml("<Attributes root=\"1\" bass=\"0\" intervals=\"1 0 0 0 3 0 0 B 5 0 7 D\"/>");
        const auto node = xml.getDocumentElement();
        if (!node || decodeStudioOneChord(*node).isEmpty())
            fail("Smart Improviser chord decoding with hex degrees and bass failed");
    }
    const auto parsed = readStudioOneShowXml(fixture);
    if (parsed.items.size() != 2) fail("Studio One reader must keep both source items");
    const auto& first = parsed.items[0];
    const auto& second = parsed.items[1];
    if (first.sourceItemOrdinal != 0 || second.sourceItemOrdinal != 1 ||
        first.sourceItemTitle != "Same name" || second.sourceItemTitle != "Same name")
        fail("Studio One source ordinal, order or duplicate title was lost");
    if (!first.bpm || std::abs(*first.bpm - 120.) > 1.e-6 ||
        !first.lengthBars || std::abs(*first.lengthBars - 8.) > 1.e-6 ||
        first.sourceMeter != "4/4" || first.sourceSectionCount != 1 ||
        first.sourceChordCount != 1 || !first.sections.isArray() ||
        first.sections.size() != 1 || first.rawChords.size() != 1 ||
        !first.chords.isArray() || first.chords.size() != 1 ||
        first.chords[0]["symbol"].toString() != "C7")
        fail("Studio One Show raw chords, seconds-to-bars or section preview is invalid");
    if (std::abs(static_cast<double>(first.sections[0]["length"]) - 4.) > 1.e-6)
        fail("Studio One section length did not convert from seconds to bars");
    if (!second.bpm || std::abs(*second.bpm - 80.) > 1.e-6 ||
        !second.lengthBars || std::abs(*second.lengthBars - 4.) > 1.e-6 ||
        second.sections.isArray() || second.warnings.isEmpty())
        fail("Incompatible source sections must be unavailable with an explanation");
    auto state = empty();
    const auto target = addItem(state, "Selected target");
    insertEntry(state, target);
    const auto other = addItem(state, "Other target");
    insertEntry(state, other);
    auto source = first;
    source.targetItemId = target;
    const auto before = state.clone();
    applyShowContext(state, source, {.sections = true, .tempo = true, .chords = true, .length = true});
    if (!sameShowValue(find(state,"items",other),find(before,"items",other)) ||
        std::abs(static_cast<double>(find(state,"items",target)["tempo"]) - 120.) > 1.e-6 ||
        musicalRows(state,target,MusicalLane::sections).size() != 1 ||
        musicalRows(state,target,MusicalLane::chords).size() != 1)
        fail("Source import must only modify explicitly mapped MR Live target");
    const auto applied = state.clone();
    // Unsupported extra source interval must not partially alter the target.
    const auto malformed = readStudioOneShowXml(
        "<Show><Attributes x:id=\"Model\"><Attributes x:id=\"setList\">"
        "<List x:id=\"Items\"><SetListItem tempo=\"0.5\" length=\"8\" "
        "timeSignature.numerator=\"4\" timeSignature.denominator=\"4\">"
        "<List x:id=\"chords\"><SetListItemChord timeFormat=\"0\" localOffset=\"0\" length=\"2\">"
        "<Attributes x:id=\"chord\" root=\"0\" intervals=\"1 0 0 0 3 0 0 5 0 0 7 Z\"/>"
        "</SetListItemChord></List></SetListItem></List></Attributes></Attributes></Show>");
    if (malformed.items.size() != 1 || !malformed.items[0].chords.isVoid() ||
        malformed.items[0].warnings.isEmpty())
        fail("Invalid encoded chord should disable only its source category");
    auto badSource = malformed.items[0];
    badSource.targetItemId = target;
    try {
        applyShowContext(state, badSource, {.chords = true});
        fail("Invalid source chord must fail closed");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("unavailable")) throw;
    }
    if (!sameShowValue(state,applied)) fail("Rejected chord import mutated the show");
    put(require(state,"items",target),"meter","3/4");
    const auto beforeMeter = state.clone();
    try {
        applyShowContext(state, source, {.sections = true});
        fail("Different source/target meters were silently accepted");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("meter")) throw;
    }
    if (!sameShowValue(state,beforeMeter)) fail("Meter mismatch must be atomic");
    try {
        (void)readStudioOneShowXml("<!DOCTYPE Show [<!ENTITY x 'bad'>]><Show/>");
        fail("Entity declarations must be rejected");
    } catch (const std::runtime_error& error) {
        if (!juce::String(error.what()).contains("DTD")) throw;
    }
}
} // namespace ui::live::test
