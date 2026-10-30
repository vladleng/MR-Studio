#pragma once
#include "../apps/studio-juce/src/LiveTimelineEditState.h"

namespace ui::live::test {
inline void timelineEditing() {
    const auto check=[](bool pass,const char *message){if(!pass)throw std::runtime_error(message);};
    auto state=empty();const auto item=addItem(state,"Timeline");
    put(require(state,"items",item),"meter","4/4");
    auto chords=newMusicalSpan(state,item,MusicalLane::chords,2.,32.);
    const auto first=chords[0]["id"].toString();
    check(static_cast<double>(chords[0]["length"])==1.,"Chord default covers four quarter notes");
    setMusicalRows(state,item,MusicalLane::chords,chords);
    chords=newMusicalSpan(state,item,MusicalLane::chords,4.,32.);
    const auto before=canonical(chords);
    auto moved=editMusicalSpan(chords,first,100.,SpanGesture::move,MusicalLane::chords);
    check(static_cast<double>(spanById(moved,first)["start"])==3.,"Chord move clamps before next neighbour");
    auto trimmed=editMusicalSpan(chords,first,100.,SpanGesture::left,MusicalLane::chords);
    check(static_cast<double>(spanById(trimmed,first)["length"])==.25,"Chord left trim retains quarter-bar minimum");
    trimmed=editMusicalSpan(chords,first,100.,SpanGesture::right,MusicalLane::chords);
    check(static_cast<double>(spanById(trimmed,first)["length"])==2.,"Chord right trim clamps to next neighbour");
    bool rejected=false;try{editMusicalSpan(chords,first,0.,SpanGesture::move,MusicalLane::chords,true);}catch(...){rejected=true;}
    check(rejected && canonical(chords)==before,"Overlapping Alt-copy fails without mutating original lane");
    auto copied=editMusicalSpan(chords,first,5.,SpanGesture::move,MusicalLane::chords,true);
    check(copied.size()==3 && copied[2]["id"]!=chords[0]["id"] && canonical(chords)==before,"Valid Alt-copy has a fresh ID; original remains intact");
    auto sections=newMusicalSpan(state,item,MusicalLane::sections,0.,32.,"Intro");
    setMusicalRows(state,item,MusicalLane::sections,sections);
    sections=newMusicalSpan(state,item,MusicalLane::sections,8.,32.,"Chorus");
    check(sections[1]["color"].toString()=="violet","Section templates preserve accepted color defaults");
    for(const auto lane:{MusicalLane::sections,MusicalLane::chords}){
        const auto a=sections[0]["id"].toString(),b=sections[1]["id"].toString();
        auto contiguous=sections.clone();
        if(lane==MusicalLane::chords)for(auto& row:*contiguous.getArray())put(row,"symbol","Am");
        const auto target=musicalHit(contiguous,8.01,.15);
        check(target.id==a && target.mode==SpanGesture::joined,"Both sides of a seam resolve to one shared boundary");
        auto joined=editMusicalSpan(contiguous,a,2.,SpanGesture::joined,lane);
        check(static_cast<double>(spanById(joined,a)["length"])==10. &&
              static_cast<double>(spanById(joined,b)["start"])==10. && static_cast<double>(spanById(joined,b)["length"])==6.,
              "Joined resize preserves outer boundaries without gaps/overlap");
        joined=editMusicalSpan(contiguous,a,100.,SpanGesture::joined,lane);
        check(static_cast<double>(spanById(joined,b)["length"])==.25,"Shared boundary respects neighbour minimum length");
        check(musicalHit(contiguous,-.01,.15).mode==SpanGesture::left && musicalHit(contiguous,16.01,.15).mode==SpanGesture::right,
              "Outer edge hit areas include a small margin outside event");
        auto overlap=contiguous.clone();put(overlap.getArray()->getReference(1),"start",7.);
        rejected=false;try{validateMusicalRows(overlap,lane);}catch(...){rejected=true;}
        check(rejected,"Both Sections and Chords reject overlapping properties");
        const auto clamped=editMusicalSpan(contiguous,a,10.,SpanGesture::move,lane);
        check(static_cast<double>(spanById(clamped,a)["start"])==0.,"Both lanes clamp drag against neighbours");
    }
    setMusicalRows(state,item,MusicalLane::sections,sections);
    setMusicalRows(state,item,MusicalLane::chords,copied);
    const auto source=canonical(find(state,"items",item));
    const auto events=canonical(musicalRows(state,item,MusicalLane::chords));
    setItemView(state,item,"lead",4.);setItemView(state,item,"scale",32.);setItemView(state,item,"length",1.);
    check(canonical(find(state,"items",item))==source && canonical(musicalRows(state,item,MusicalLane::chords))==events,
          "Shortening item or changing origin/zoom never rewrites source bars or hidden events");
    setItemView(state,item,"length",32.);
    check(canonical(musicalRows(state,item,MusicalLane::chords))==events,"Extending item restores hidden events unchanged");
    const auto restored=juce::JSON::parse(juce::JSON::toString(state));
    check(canonical(restored)==canonical(state),"Timeline authoring data round-trips existing show JSON");
    check(barSnap(1.13)==1.25 && barSnap(1.12)==1.,"All musical gestures use quarter-bar snap");
    rejected=false;try{setItemView(state,item,"length",0.);}catch(...){rejected=true;}
    check(rejected,"Item length cannot be zero");
}
} // namespace ui::live::test
