// Static fixture illustrations. No audio, clock, DSP, or executable imported content.
const el = (tag, className, text) => {
  const node = document.createElement(tag);
  node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
};
const add = (parent, className, text, tag = 'div') => {
  const node = el(tag, className, text);
  parent.append(node);
  return node;
};
function asset(parent, assets, id, className, label) {
  const path = assets.get(id);
  if (!path) return add(parent, 'visual-missing', label);
  const img = el('img', className);
  img.src = path;
  img.alt = label;
  parent.append(img);
  return img;
}
const actionLabels = {
  'transport.previous': 'Предыдущая песня', 'transport.play': 'Play',
  'transport.pause': 'Pause', 'transport.stop': 'Stop', 'transport.next': 'Следующая песня',
  'show.open': 'Открыть', 'show.save': 'Сохранить', 'view.mixer': 'Микшер', 'view.patches': 'Патчи'
};
const actionAssets = {
  'transport.previous': 'icons/chevron_left', 'transport.play': 'transport/play',
  'transport.pause': 'transport/pause', 'transport.stop': 'transport/stop',
  'transport.next': 'icons/chevron_right', 'show.open': 'icons/concept_pack/folder_open',
  'show.save': 'icons/save', 'view.mixer': 'icons/settings', 'view.patches': 'icons/routing'
};
function actions(parent, block, assets, playing) {
  const row = add(parent, 'visual-actions');
  row.style.flexDirection = block.direction;
  for (const binding of block.bindings) {
    const control = add(row, `visual-action${playing && binding === 'transport.play' ? ' active' : ''}`);
    control.title = `${actionLabels[binding]} · визуальный пример`;
    control.setAttribute('aria-label', control.title);
    asset(control, assets, actionAssets[binding], 'visual-icon', actionLabels[binding]);
    if (block.type === 'control.toolbar') add(control, 'action-label', actionLabels[binding], 'span');
  }
}
export function renderVisual(node, block, fixture, assets) {
  const root = add(node, `visual visual-${block.type.replace('.', '-')}`);
  const playing = fixture === 'playing';
  const title = fixture === 'long'
    ? 'Очень длинное название песни и концертной программы — Moon River Studio Live Session'
    : 'Moon River';
  root.setAttribute('aria-label', 'Визуальный пример, демонстрационные данные');
  if (fixture === 'empty') {
    add(root, 'visual-empty', 'Шоу пока пусто');
    add(root, 'visual-caption', 'Добавьте песни, stems и патчи');
    if (block.type.startsWith('control.')) actions(root, block, assets, false);
    return;
  }
  switch (block.type) {
    case 'show.setlist': {
      [title, 'Fly Me to the Moon', 'Autumn Leaves'].forEach((name, i) => {
        const row = add(root, `song-row${i === 0 ? ' current' : ''}`);
        add(row, 'song-number', String(i + 1).padStart(2, '0'));
        const text = add(row, 'song-text');
        add(text, 'song-name', name);
        add(text, 'visual-caption', i === 0 ? 'CURRENT · 92 BPM · C major' : `${i === 1 ? 'NEXT' : 'READY'} · 4/4`);
        add(row, 'song-time', ['03:42', '04:15', '05:08'][i]);
      });
      break;
    }
    case 'song.header': {
      add(root, 'visual-caption', '01 / 12 · EVENING SESSION');
      add(root, 'song-display', title);
      const row = add(root, 'visual-chips');
      for (const value of ['92 BPM', '4/4', 'C major', playing ? 'PLAYING' : 'READY']) add(row, 'visual-chip', value);
      break;
    }
    case 'song.timeline': {
      const ruler = add(root, 'timeline-ruler');
      for (const bar of ['1', '5', '9', '13', '17', '21']) add(ruler, '', bar);
      const sections = add(root, 'timeline-sections');
      for (const [i, name] of ['Intro', 'Verse', 'Chorus', 'Outro'].entries()) add(sections, i === 1 ? 'current' : '', name);
      const lane = add(root, 'timeline-lane');
      asset(lane, assets, 'controls/concept_pack/waveform_audio_illustration', 'visual-wave', 'Демонстрационная волна');
      add(lane, 'timeline-cursor');
      const patches = add(root, 'timeline-patches');
      add(patches, 'visual-chip', 'A · 3.2'); add(patches, 'visual-chip', 'B · 9.3');
      break;
    }
    case 'playback.stems': {
      const names = fixture === 'many' ? Array.from({length:16}, (_, i) => `Stem ${i + 1}`) : ['Drums', 'Bass', 'Keys', 'Backing vocals'];
      names.forEach((name, i) => {
        const row = add(root, 'stem-row');
        add(row, 'stem-name', name);
        asset(row, assets, `controls/concept_pack/waveform_${i === 3 ? 'vocal' : 'instrument'}_illustration`, 'visual-wave', 'Демонстрационная волна');
        add(row, 'visual-caption', 'M  S');
      });
      break;
    }
    case 'input.patches': {
      for (const [i, name] of ['Vocal', 'Guitar'].entries()) {
        const row = add(root, 'patch-row');
        asset(row, assets, i ? 'icons/pedal' : 'icons/mic', 'visual-icon', name);
        const text = add(row, 'patch-text');
        add(text, '', name); add(text, 'visual-caption', i ? 'Input 02 → Rig' : 'Input 01 → Vocal bus');
        add(row, 'visual-chip', i ? 'Warm lead' : 'Clean');
      }
      break;
    }
    case 'mixer.channels': {
      const channels = add(root, 'mixer-strips');
      ['Vocal', 'Guitar', 'Playback', 'Main'].forEach((name) => {
        const strip = add(channels, 'mixer-strip');
        add(strip, 'channel-name', name);
        const controls = add(strip, 'channel-controls');
        asset(controls, assets, 'controls/concept_pack/fader_micro_normal', 'visual-fader', 'MR Micro · статический пример');
        asset(controls, assets, `controls/concept_pack/${playing ? 'meter_mid' : 'meter_empty'}`, 'visual-meter', 'Meter · демонстрационные данные');
        add(strip, 'visual-caption', 'GAIN · пример');
        add(strip, 'channel-switches', 'M     S');
      });
      break;
    }
    case 'control.transport': {
      const readout = add(root, 'transport-readout');
      add(readout, 'transport-time', '001 : 01 : 000');
      add(readout, 'visual-caption', `${playing ? 'PLAYING' : 'STOPPED'} · 00:00 / 03:42 · 92 BPM`);
      actions(root, block, assets, playing);
      break;
    }
    case 'control.toolbar':
      actions(root, block, assets, false);
      add(root, 'visual-caption', 'SHOW / EDIT / ROUTING');
      break;
    case 'musical.chords': {
      const strip = add(root, 'chord-strip');
      ['Dm7', 'G7', 'Cmaj7', 'Am7'].forEach((chord, i) => {
        const card = add(strip, i === 1 ? 'chord-card current' : 'chord-card');
        add(card, 'visual-caption', ['PREVIOUS', 'CURRENT', 'NEXT', ''][i]);
        add(card, 'chord-symbol', chord);
      });
      add(root, 'visual-caption', '13.1    •    13.2    •    13.3    •    13.4');
      break;
    }
    case 'musical.sections': {
      const row = add(root, 'section-pair');
      for (const [label, name] of [['CURRENT', 'Verse 2'], ['NEXT', 'Chorus']]) {
        const part = add(row, 'section-card');
        add(part, 'visual-caption', label); add(part, 'section-name', name);
      }
      const progress = add(root, 'section-progress'); add(progress, 'section-progress-fill');
      add(root, 'visual-caption', '8 / 16 bars · демонстрация');
      break;
    }
    case 'info.cues':
      add(root, 'cue-count', '4 такта');
      add(root, 'cue-message', 'Вступление вокала');
      add(root, 'visual-caption', 'NEXT · Chorus / Cue 03');
      break;
    case 'info.lyrics':
      add(root, 'lyric-past', 'Moon river, wider than a mile');
      add(root, 'lyric-current', 'I’m crossing you in style someday');
      add(root, 'lyric-next', 'Oh, dream maker, you heart breaker…');
      break;
    case 'system.status':
      for (const [name, value] of [['Audio', '48 kHz / 128'], ['MIDI', '2 ports'], ['Preflight', 'READY']]) {
        const row = add(root, 'status-row'); add(row, 'status-dot');
        add(row, '', name); add(row, 'visual-caption', `${value} · demo`);
      }
      break;
    default: add(root, 'visual-caption', 'Fixture preview');
  }
}
