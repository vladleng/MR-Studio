import {studioMenus} from './studio-menus.mjs';
import {recordButton} from './recording-controls.mjs';
import {transport} from './recording-runtime.mjs';
import {renderChannelPanel} from './channels-ui.mjs';
import {renderShowBrowser,activeItem,showDocument,editShow} from './show-browser.mjs';
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
  img.dataset.slot = id;
  img.alt = label;
  parent.append(img);
  return img;
}
const actionLabels = {
  'transport.previous': 'Предыдущая песня', 'transport.play': 'Play',
  'transport.pause': 'Pause', 'transport.stop': 'Stop', 'transport.next': 'Следующая песня',
  'show.open': 'Открыть', 'show.save': 'Сохранить', 'view.mixer': 'Микшер', 'view.patches': 'Browser', 'view.browser':'Browser'
};
const actionAssets = {
  'transport.previous': 'icons/chevron_left', 'transport.play': 'transport/play',
  'transport.pause': 'transport/pause', 'transport.stop': 'transport/stop',
  'transport.next': 'icons/chevron_right', 'show.open': 'icons/concept_pack/folder_open',
  'show.save': 'icons/save', 'view.mixer': 'icons/concept_pack/mixer_params', 'view.patches': 'icons/concept_pack/folder_open', 'view.browser':'icons/concept_pack/folder_open'
};
function actions(parent, block, assets, playing) {
  const row = add(parent, 'visual-actions');
  row.style.flexDirection = block.direction;
  for (const binding of block.bindings) {
    if(block.type==='control.toolbar'&&binding.startsWith('show.'))continue;
    const control = add(row, `visual-action${playing && binding === 'transport.play' ? ' active' : ''}`);
    control.dataset.action=binding;
    control.title = `${actionLabels[binding]} · визуальный пример`;
    control.setAttribute('aria-label', control.title);
    asset(control, assets, actionAssets[binding], 'visual-icon', actionLabels[binding]);
    if (block.type === 'control.toolbar'&&!binding.startsWith('view.')) add(control, 'action-label', actionLabels[binding], 'span');
    if(binding.startsWith('view.'))control.classList.add('icon-only');
  }
}
function slider(parent, label, kind, value, min, max) {
  const row=add(parent, `level-control ${kind}-control`);
  const input=add(row, `${kind}-slider`, undefined, 'input');
  input.type='range';input.min=min;input.max=max;input.step=kind==='pan'?1:.5;input.value=value;input.disabled=true;
  input.setAttribute('aria-label',label);
  add(row, `${kind}-value`, kind==='pan'?'C':`${value} dB`, 'output');
}
function menuButton(parent,label,kind){const button=add(parent,'strip-menu',label,'button');button.type='button';button.disabled=true;button.dataset.menu=kind;return button;}
export function renderVisual(node, block, fixture, assets) {
  const root = add(node, `visual visual-${block.type.replace('.', '-')}`);
  if(block.type==='show.setlist'){renderShowBrowser(root,block.id,document.getElementById('preview').checked);return;}
  const item=activeItem();
  const playing = fixture === 'playing';
  const title = fixture === 'long'
    ? 'Очень длинное название песни и концертной программы — Moon River Studio Live Session'
    : (item?.name||'Выберите item');
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
      for (const value of [`${item?.tempo||92} BPM`, item?.meter||'4/4',item?.key||'C', playing ? 'PLAYING' : 'READY']) add(row, 'visual-chip', value);
      break;
    }
    case 'song.timeline': {renderChannelPanel(root,'timeline',assets);break;}
    case 'playback.stems': {
      renderChannelPanel(root,'playback',assets);
      break;
    }
    case 'input.patches': {
      renderChannelPanel(root,'inputs',assets);
      break;
    }
    case 'mixer.channels': {
      renderChannelPanel(root,'mixer',assets);
      break;
    }
    case 'control.transport': {
      const readout = add(root, 'transport-readout');
      add(readout, 'transport-time', '001 : 01 : 000');
      add(readout, 'visual-caption', `${playing ? 'PLAYING' : 'STOPPED'} · 00:00 / 03:42 · 92 BPM`);
      actions(root, block, assets, document.getElementById('preview').checked?transport.playing:playing);
      recordButton(root.querySelector('.visual-actions'),assets,document.getElementById('preview').checked);
      if(document.getElementById('preview').checked)readout.querySelector('.visual-caption').textContent=transport.notice||`${transport.recording?'RECORDING':transport.playing?'PLAYING':'STOPPED'} · демо, без аудио`;
      break;
    }
    case 'control.toolbar':
      studioMenus(root,document.getElementById('preview').checked);const panels=add(root,'toolbar-panels');for(const [key,label,id] of [['playback','Playback / stems','icons/wave'],['status','Status','icons/routing']]){const button=add(panels,'toolbar-panel-icon',undefined,'button');button.type='button';button.title=label;button.setAttribute('aria-label',label);button.setAttribute('aria-pressed',String(showDocument().performance.panels[key]));button.disabled=!document.getElementById('preview').checked;asset(button,assets,id,'visual-icon',label);button.onclick=()=>editShow(d=>d.performance.panels[key]=!d.performance.panels[key]);}
      actions(root, block, assets, false);

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
      add(root, 'cue-message', item?.notes||'Вступление вокала');
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
