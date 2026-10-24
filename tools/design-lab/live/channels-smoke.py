from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===6');page.locator('#preview').check()
 container=page.locator('.live-item-container').first;container.get_by_role('button',name='+ Канал',exact=True).click();dialog=page.get_by_role('dialog');dialog.get_by_label('Тип канала',exact=True).select_option('Player');dialog.get_by_label('Название',exact=True).fill('Lead');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 doc=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'));cid=doc['channels'][-1]['id'];assert all(any(t['channelId']==cid for t in i['tracks']) for i in doc['items'])
 row=container.locator(f'.item-track[data-channel="{cid}"]');row.locator('.channel-dots').click();page.get_by_role('menu').get_by_role('menuitem',name='Переименовать канал',exact=True).click();dialog=page.get_by_role('dialog');dialog.get_by_label('Название',exact=True).fill('Lead 2');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer');assert mixer.locator('.mixer-strip').count()==5;assert 'Lead 2' in mixer.inner_text()
 vocal=mixer.locator('.mixer-strip').filter(has=page.get_by_role('slider',name='Громкость Vocal',exact=True));vocal.get_by_role('button',name='Запись: Vocal',exact=True).click();assert 'Готов к записи' in container.inner_text();mixer.get_by_role('button',name='Закрыть',exact=True).click()
 row.locator('.channel-dots').click();page.get_by_role('menu').get_by_role('menuitem',name='Удалить канал',exact=True).click();page.get_by_role('dialog').get_by_role('button',name='Сохранить',exact=True).click();assert container.locator('.item-track').count()==4
 root=page.locator('.show-browser');root.locator('.show-rows').dispatch_event('contextmenu',{'clientX':100,'clientY':250});page.get_by_role('menu').get_by_role('menuitem',name='Undo show',exact=True).click();assert container.locator('.item-track').count()==5
 page.reload();page.wait_for_selector('.item-track');assert page.locator('.live-item-container').first.locator('.item-track').count()==5;assert not errors,errors;b.close()
print('PASS: shared channel tracks, rename/mixer identity, audio arm, delete/Undo/reload')
