from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===6');page.locator('#preview').check()
 assert page.locator('.block[aria-label="Inputs / patches"]').count()==0
 assert page.locator('.block .mixer-strip').count()==0
 container=page.locator('.live-item-container');assert container.locator('.item-track').count()==4
 fold=container.locator('.item-fold');fold.click();assert container.locator('.item-track').count()==0;fold.click()
 container.get_by_role('button',name='+ Канал',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Тип канала',exact=True).select_option('Player');dialog.get_by_label('Название',exact=True).fill('Lead');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 assert container.locator('.item-track').count()==5
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));cid=next(c['id'] for c in doc['channels'] if c['name']=='Lead')
 row=container.locator(f'.item-track[data-channel="{cid}"]');row.get_by_role('button',name='+ Патч',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Название патча',exact=True).fill('Clean');dialog.get_by_label('Цвет патча',exact=True).select_option('violet');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 assert row.locator('.track-patch').count()==2
 card=row.locator('.track-patch').filter(has=page.get_by_role('button',name='Патч Lead: Clean',exact=True))
 for name,color,volume in [('Rhythm','green','0'),('Solo','amber','3')]:
  card.get_by_role('button',name='+ Sub-patch',exact=True).click()
  dialog=page.get_by_role('dialog');dialog.get_by_label('Название sub-patch',exact=True).fill(name);dialog.get_by_label('Цвет sub-patch',exact=True).select_option(color)
  dialog.get_by_label('Изменять volume',exact=True).check();dialog.get_by_label('Значение volume',exact=True).fill(volume);dialog.get_by_role('button',name='Сохранить',exact=True).click()
 assert card.locator('.track-subpatch').count()==2
 engine=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))['channels'][-1]['patches'][-1]['engine']
 card.get_by_role('button',name='Sub-patch Lead: Solo',exact=True).click()
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));patch=doc['channels'][-1]['patches'][-1]
 assert patch['color']=='violet' and patch['subPatches'][1]['color']=='amber' and patch['engine']==engine
 page.locator('[data-action="view.mixer"]').click()
 mixer=page.get_by_role('region',name='Микшер MR Live',exact=True);assert mixer.locator('.mixer-strip').count()==5
 gain=mixer.get_by_role('slider',name='Громкость Lead',exact=True);gain.fill('-9');gain.dispatch_event('input');gain.dispatch_event('change')
 assert json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))['channels'][-1]['gain']==-9
 page.locator('[data-action="view.mixer"]').click();assert mixer.count()==0
 page.locator('[data-action="view.browser"]').click()
 browser=page.get_by_role('region',name='Browser MR Studio',exact=True)
 assert browser.get_by_role('button',name='FX',exact=True).count()==1
 assert 'AI' not in browser.inner_text()
 browser.get_by_role('button',name='Files',exact=True).click();assert 'Backing.wav' in browser.inner_text()
 browser.get_by_role('button',name='MR Library',exact=True).click();assert 'Collections' in browser.inner_text()
 browser.get_by_role('button',name='FX',exact=True).click();browser.get_by_label('Канал Browser').select_option(cid);browser.get_by_role('button',name='MR EQ',exact=True).dblclick()
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));assert 'MR EQ' in doc['channels'][-1]['patches'][-1]['engine']['plugins']
 page.get_by_role('region',name='Browser MR Studio',exact=True).get_by_role('button',name='Закрыть',exact=True).click()
 page.reload();page.wait_for_selector('.live-item-container');page.locator('#preview').check();assert page.get_by_role('button',name='Sub-patch Lead: Solo',exact=True).get_attribute('aria-pressed')=='true'
 page.locator('#fullscreen-preview').click();page.wait_for_selector('.fullscreen-preview');page.locator('[data-action="view.mixer"]').click();assert page.get_by_role('region',name='Микшер MR Live',exact=True).is_visible();page.locator('[data-action="view.browser"]').click()
 mix=page.locator('.runtime-mixer').bounding_box();browse=page.locator('.runtime-browser').bounding_box();assert mix['x']+mix['width']<=browse['x']+1
 page.locator('.runtime-browser').get_by_role('button',name='Закрыть',exact=True).click();page.locator('.runtime-mixer').get_by_role('button',name='Закрыть',exact=True).click();page.locator('#exit-preview').click()
 assert not errors,errors
 page.screenshot(path='/tmp/mr-live-item-container.png');b.close()
print('PASS: grouped item, patch/subpatch rectangles+colors, channel CRUD, icon mixer/browser, FX/Files/Library, persistence, fullscreen')
