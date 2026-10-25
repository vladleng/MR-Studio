from playwright.sync_api import sync_playwright
import json,math
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=browser.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===5');page.locator('#preview').check()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 # Trigger a harmless library command so initial demo state is persisted.
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click()
 d=doc();entries=d['setlists'][0]['entries'];assert page.locator('.live-item-container').count()==len(entries)
 left=page.locator('.live-item-container').nth(0).bounding_box();right=page.locator('.live-item-container').nth(1).bounding_box();assert right['x']>=left['x']+left['width']-1
 assert page.locator('.live-item-container').evaluate_all('(nodes)=>nodes.map(n=>n.dataset.entry)')==[e['id'] for e in entries]
 first=page.locator(f'.live-item-container[data-entry="{entries[0]["id"]}"]');itemid=entries[0]['itemId'];item=next(i for i in d['items'] if i['id']==itemid)
 backing=next(c for c in d['channels'] if c['type']=='Playback');player=next(c for c in d['channels'] if c['type']=='Player')
 assert first.locator(f'.item-track[data-channel="{backing["id"]}"] .track-patch').count()==0
 assert first.locator(f'.item-track[data-channel="{backing["id"]}"] .audio-file-name').count()==1
 stems=page.locator('.visual-playback-stems')
 with page.expect_file_chooser() as chosen:stems.get_by_role('button',name='Выбрать файл…',exact=True).click()
 chosen.value.set_files({'name':'Concert Backing.wav','mimeType':'audio/wav','buffer':b'demo metadata only'})
 assert 'Concert Backing.wav' in first.locator(f'.item-track[data-channel="{backing["id"]}"]').inner_text()
 gain=stems.get_by_role('slider',name='Громкость Backing',exact=True);gain.fill('-11');gain.dispatch_event('input');gain.dispatch_event('change');assert next(c for c in doc()['channels'] if c['id']==backing['id'])['gain']==-11
 row=first.locator(f'.item-track[data-channel="{player["id"]}"]');card=row.locator('.track-patch').first
 for name in ['Rhythm','Solo']:
  card.get_by_role('button',name='+ Sub-patch',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Название sub-patch',exact=True).fill(name);modal.get_by_role('button',name='Сохранить',exact=True).click()
 d=doc();t=next(t for i in d['items'] if i['id']==itemid for t in i['tracks'] if t['channelId']==player['id']);patch=next(c for c in d['channels'] if c['id']==player['id'])['patches'][0];pid=patch['id'];sid=patch['subPatches'][1]['id'];tid=t['id']
 def span():return next(t for i in doc()['items'] if i['id']==itemid for t in i['tracks'] if t['id']==tid)['spans'][pid]
 def drag(target,mode,delta,limit):
  parent=target.locator('..').bounding_box();px=parent['width']/limit
  handle=target.locator(':scope > .extent-'+mode) if mode in ['left','right'] else target
  if mode=='move' and 'temporal-patch' in (target.get_attribute('class') or ''):handle=target.locator('.track-patch-title')
  handle.scroll_into_view_if_needed();box=handle.bounding_box();x=box['x']+box['width']/2;y=box['y']+box['height']/2
  page.mouse.move(x,y);page.mouse.down();page.mouse.move(x+delta*px,y,steps=8);page.mouse.up()
 old=span();drag(card,'left',4,item['bars']);assert math.isclose(span()['start'],4);assert math.isclose(span()['length'],old['length']-4)
 old=span();drag(card,'right',-8,item['bars']);assert math.isclose(span()['length'],old['length']-8)
 sub=card.locator(f'[data-subpatch="{sid}"]');old=span()['subs'][sid];drag(sub,'move',-2,span()['length']);assert math.isclose(span()['subs'][sid]['start'],old['start'],abs_tol=.25)
 old=span()['subs'][sid];drag(sub,'right',-2,span()['length']);assert math.isclose(span()['subs'][sid]['length'],old['length']-2,abs_tol=.25)
 page.keyboard.press('Control+z');assert math.isclose(span()['subs'][sid]['length'],old['length'],abs_tol=.25)
 page.keyboard.press('Control+Shift+z');assert math.isclose(span()['subs'][sid]['length'],old['length']-2,abs_tol=.25)
 part=first.locator('.song-part').first;partid=part.get_attribute('data-section');before=next(s for i in doc()['items'] if i['id']==itemid for s in i['sections'] if s['id']==partid)['start'];drag(part,'move',2,item['bars']);assert math.isclose(next(s for i in doc()['items'] if i['id']==itemid for s in i['sections'] if s['id']==partid)['start'],before+2,abs_tol=.25)
 part.click(button='right');page.get_by_role('menu').get_by_role('menuitem',name='Свойства / цвет части…',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Цвет части',exact=True).select_option('red');modal.get_by_role('button',name='Сохранить',exact=True).click();assert page.get_by_role('dialog').count()==0
 root=page.locator('.block .show-browser');root.locator('.song-row').first.click(button='right');page.get_by_role('menu').get_by_role('menuitem',name='↓',exact=True).click()
 d=doc();assert page.locator('.live-item-container').evaluate_all('(nodes)=>nodes.map(n=>n.dataset.entry)')==[e['id'] for e in d['setlists'][0]['entries']]
 root.get_by_role('button',name='Browser',exact=True).click();root=page.locator('.runtime-items .show-browser');root.locator('.song-row').first.click(button='right');page.get_by_role('menu').get_by_role('menuitem',name='Добавить в setlist',exact=True).click();assert page.locator('.live-item-container').count()==4
 saved=doc();page.reload();page.wait_for_selector('.live-item-container');assert page.locator('.live-item-container').count()==4
 assert doc()==saved;assert not errors,errors;page.screenshot(path='/tmp/mr-live-timeline.png');browser.close()
print('PASS: setlist order/repetition, media-only Backing/gain, patch left/right resize, sub move/resize, song-section move/color, reload')
