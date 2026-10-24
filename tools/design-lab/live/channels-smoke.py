from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=browser.new_page(viewport={'width':1920,'height':1200});errors=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===8');page.locator('#preview').check()
 inputs=page.locator('[data-id=block-5]');timeline=page.locator('[data-id=block-3]');mixer=page.locator('.visual-mixer-channels')
 assert inputs.locator('.linked-channel-row').count()==4
 assert timeline.locator('.channel-tracks .linked-channel-row').count()==4
 assert mixer.locator('.mixer-strip').count()==4
 inputs.get_by_role('button',name='+ Канал',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Тип канала',exact=True).select_option('Player');dialog.get_by_label('Название',exact=True).fill('Lead');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 assert inputs.locator('.linked-channel-row').count()==5
 assert timeline.locator('.channel-tracks .linked-channel-row').count()==5
 assert mixer.locator('.mixer-strip').count()==5
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));cid=next(c['id'] for c in doc['channels'] if c['name']=='Lead')
 assert all(any(t['channelId']==cid for t in i['tracks']) for i in doc['items'])
 row=inputs.locator(f'.linked-channel-row[data-channel="{cid}"]')
 row.click(button='right');page.get_by_role('menu').get_by_role('menuitem',name='+ Патч',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Название патча',exact=True).fill('Clean');dialog.get_by_label('Плагины (через запятую)',exact=True).fill('MR Rig, MR Reverb');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 inputs.get_by_label('Патч Lead',exact=True).select_option(label='Clean')
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));channel=next(c for c in doc['channels'] if c['id']==cid);patch=next(p for p in channel['patches'] if p['name']=='Clean');engine=patch['engine']
 row.click(button='right');page.get_by_role('menu').get_by_role('menuitem',name='+ Sub-patch',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Название sub-patch',exact=True).fill('Solo');dialog.get_by_label('Изменять volume',exact=True).check();dialog.get_by_label('Значение volume',exact=True).fill('3');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 inputs.get_by_label('Sub-patch Lead',exact=True).select_option(label='Solo')
 assert timeline.get_by_label('Sub-patch Lead',exact=True).input_value()==inputs.get_by_label('Sub-patch Lead',exact=True).input_value()
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));channel=next(c for c in doc['channels'] if c['id']==cid);patch=next(p for p in channel['patches'] if p['name']=='Clean')
 assert patch['engine']==engine
 assert patch['parameters']['volume']==0
 assert patch['subPatches'][0]['overrides']=={'volume':3}
 assert all(t['subPatchId'] is None for i in doc['items'] if i['id']!=doc['currentItem'] for t in i['tracks'] if t['channelId']==cid)
 gain=inputs.get_by_role('slider',name='Громкость Lead',exact=True);gain.fill('-12');gain.dispatch_event('input');gain.dispatch_event('change')
 assert mixer.get_by_role('slider',name='Громкость Lead',exact=True).input_value()=='-12'
 pan=mixer.get_by_role('slider',name='Панорама Lead',exact=True);pan.fill('30');pan.dispatch_event('input');pan.dispatch_event('change')
 assert inputs.get_by_role('slider',name='Панорама Lead',exact=True).input_value()=='30'
 page.reload();page.wait_for_selector('.block');page.locator('#preview').check()
 assert inputs.get_by_label('Sub-patch Lead',exact=True).locator('option:checked').inner_text()=='Solo'
 row.click(button='right');page.get_by_role('menu').get_by_role('menuitem',name='Удалить канал',exact=True).click();page.get_by_role('dialog').get_by_role('button',name='Сохранить',exact=True).click()
 assert mixer.locator('.mixer-strip').count()==4
 assert timeline.locator('.channel-tracks .linked-channel-row').count()==4
 assert not errors,errors
 browser.close()
print('PASS: linked channel/track creation, per-item patch/subpatch, unchanged chain, gain/pan sync, reload, delete')
