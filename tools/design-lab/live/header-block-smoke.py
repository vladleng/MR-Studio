from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 header=page.locator('.block').filter(has=page.locator('.visual-control-header'));timeline=page.locator('.block').filter(has=page.locator('.visual-song-timeline'));assert header.count()==1;assert timeline.locator('.timeline-top').count()==0
 before=header.bounding_box();timeline.evaluate('(n)=>{n.scrollLeft=1000;n.scrollTop=300}');assert header.bounding_box()==before
 header.get_by_role('button',name='Play',exact=True).click();page.wait_for_timeout(150);assert header.bounding_box()==before;header.get_by_role('button',name='Следующий item',exact=True).click();assert header.bounding_box()==before;header.get_by_role('button',name='Stop',exact=True).click()
 timeline.evaluate('(n)=>{n.scrollLeft=0;n.scrollTop=0}')
 for name in ['Keys','Guitar','Backing','Vocal']:
  row=first.locator('.item-track[data-channel]').filter(has=page.get_by_role('button',name='Mute: '+name,exact=True));assert row.count()==1;assert row.get_by_role('button',name='Solo: '+name,exact=True).count()==1
 first.get_by_role('button',name='Mute: Keys',exact=True).click();assert doc()['channels'][0]['mute']
 page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer');assert mixer.locator('.mixer-strip[data-channel]').first.locator('[data-toggle=M]').get_attribute('aria-pressed')=='true';mixer.locator('.mixer-strip[data-channel]').first.locator('[data-toggle=S]').click();page.locator('[data-action="view.mixer"]').click();assert first.get_by_role('button',name='Solo: Keys',exact=True).get_attribute('aria-pressed')=='true'
 root=page.locator('.block .show-browser');assert root.get_by_role('combobox',name='Сэтлист',exact=True).is_visible()
 for label in ['+ Setlist','Переименовать setlist','Удалить setlist','+ Item']:assert root.get_by_role('button',name=label,exact=True).count()==0
 root.locator('.show-rows').dispatch_event('contextmenu',{'clientX':300,'clientY':500});assert page.get_by_role('menuitem',name='+ Setlist',exact=True).count()==1;assert page.get_by_role('menuitem',name='+ Item',exact=True).count()==1;page.keyboard.press('Escape')
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert header.count()==1;assert not errors,errors;b.close()
print('PASS: separate stationary Header under scroll/Play/item navigation, all track M/S and mixer sync, setlist buttons context-only, reload')
