from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check()
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 d=doc();parents=[c for c in d['channels'] if c['type'] in ['Instrument','Player']];assert len(parents)==2
 assert page.locator('.record-track').count()==0;assert not d['viewRecordTracks']
 record=page.locator('.visual-control-transport').get_by_role('button',name='Record · общая запись',exact=True);record.click();assert record.get_attribute('aria-pressed')=='false';assert 'Подготовьте' in page.locator('.transport-readout').inner_text()
 for c in parents:first.locator(f'.item-track[data-channel="{c["id"]}"]').get_by_role('button',name='Запись: '+c['name'],exact=True).click()
 assert record.get_attribute('aria-pressed')=='false'
 page.locator('[data-action="transport.play"]').click();assert record.get_attribute('aria-pressed')=='false'
 page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer');assert mixer.locator('.record-strip').count()==0
 for c in parents:assert mixer.locator(f'[data-channel="{c["id"]}"]').get_by_role('button',name='Запись: '+c['name'],exact=True).get_attribute('aria-pressed')=='true'
 mixer.get_by_role('button',name='Record · общая запись',exact=True).click();assert record.get_attribute('aria-pressed')=='true';assert 'RECORDING' in page.locator('.transport-readout').inner_text()
 mixer.get_by_role('button',name='View record tracks',exact=True).click();assert mixer.locator('.record-strip').count()==2;assert page.locator('.record-track').count()==6;assert page.locator('.record-audio-lane.recording').count()==2
 for c in parents:
  row=first.locator(f'.record-track[data-parent-channel="{c["id"]}"]');assert row.locator('.recording').count()==1
  assert row.evaluate('(n)=>n.previousElementSibling.dataset.channel')==c['id']
  strip=mixer.locator(f'[data-record-channel="{c["id"]}"]');normal=mixer.locator(f'[data-channel="{c["id"]}"]');assert strip.bounding_box()['width']<normal.bounding_box()['width']
  slider=strip.get_by_role('slider',name='Громкость записи '+c['name'],exact=True);slider.fill('-17');slider.dispatch_event('input');slider.dispatch_event('change');ch=next(ch for ch in doc()['channels'] if ch['id']==c['id']);assert ch['recordGain']==-17;assert ch['gain']==0
 page.screenshot(path='/tmp/mr-live-recording.png')
 mixer.get_by_role('button',name='Stop',exact=True).click();assert record.get_attribute('aria-pressed')=='false';assert page.locator('.record-audio-lane.recording').count()==0
 mixer.get_by_role('button',name='View record tracks',exact=True).click();assert page.locator('.record-track').count()==0;assert mixer.locator('.record-strip').count()==0
 page.keyboard.press('Control+z');assert mixer.locator('.record-strip').count()==2
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert page.locator('.record-track').count()==6;page.locator('#preview').check();assert record.get_attribute('aria-pressed')=='false'
 # Creation adds paired record lanes to every item; deleting a parent removes them, Undo restores them.
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='+ Канал',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Тип канала',exact=True).select_option('Instrument');modal.get_by_label('Название',exact=True).fill('Piano 2');modal.get_by_role('button',name='Сохранить',exact=True).click();assert page.locator('.record-track').count()==9
 channel=next(c for c in doc()['channels'] if c['name']=='Piano 2');row=first.locator(f'.item-track[data-channel="{channel["id"]}"]');row.locator('.channel-dots').click();page.get_by_role('menuitem',name='Удалить канал',exact=True).click();page.get_by_role('dialog').get_by_role('button',name='Сохранить',exact=True).click();assert page.locator('.record-track').count()==6;page.keyboard.press('Control+z');assert page.locator('.record-track').count()==9
 page.locator('#fullscreen-preview').click();assert page.locator('.record-track').count()==9;assert record.is_visible();assert not errors,errors;b.close()
print('PASS: automatic hidden recording tracks, parent R/global Record, compact independent gain, Stop, visibility/Undo/reload, channel lifecycle, fullscreen')
