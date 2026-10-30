from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check()
 panel=page.locator('.shared-track-panel');timeline=page.locator('.block').filter(has=page.locator('.visual-song-timeline'));side=page.locator('.block').filter(has=panel)
 assert panel.count()==1;assert side.get_attribute('data-id')!=timeline.get_attribute('data-id');assert abs(side.bounding_box()['x']+side.bounding_box()['width']-timeline.bounding_box()['x'])<2;assert abs(side.bounding_box()['y']-timeline.bounding_box()['y'])<2;assert abs(side.bounding_box()['height']-timeline.bounding_box()['height'])<2;assert page.locator('.live-item-container .linked-channel-title,.live-item-container .item-parts-label').count()==0
 def aligned():
  page.wait_for_timeout(180)
  assert page.evaluate('''()=>[...document.querySelectorAll('.shared-track-panel [data-channel],.shared-track-panel [data-parent-channel]')].every(label=>{const record=!!label.dataset.parentChannel,id=label.dataset.channel||label.dataset.parentChannel;return [...document.querySelectorAll(record?`.record-track[data-parent-channel="${id}"]`:`.item-track[data-channel="${id}"]`)].every(row=>Math.abs(row.getBoundingClientRect().top-label.getBoundingClientRect().top)<2&&Math.abs(row.getBoundingClientRect().height-label.getBoundingClientRect().height)<2)})''')
 aligned();x=panel.bounding_box()['x'];timeline.evaluate('(n)=>n.scrollLeft=900');assert abs(panel.bounding_box()['x']-x)<1
 timeline.evaluate('(n)=>n.scrollLeft=0');panel.get_by_role('button',name='Mute: Keys',exact=True).click();assert panel.get_by_role('button',name='Mute: Keys',exact=True).get_attribute('aria-pressed')=='true'
 page.locator('[data-action="view.mixer"]').click();assert page.locator('.runtime-mixer [data-channel]').first.locator('[data-toggle=M]').get_attribute('aria-pressed')=='true';page.locator('[data-action="view.mixer"]').click()
 page.get_by_role('button',name='View record tracks',exact=True).first.click();aligned();assert panel.locator('.shared-record-title').count()==2
 page.locator('.take-list').first.evaluate('(n)=>n.open=true');aligned()
 panel.get_by_role('button',name='Запись: Keys',exact=True).click();page.get_by_role('button',name='Record · общая запись',exact=True).first.click();page.wait_for_timeout(200);assert page.locator('.record-audio-lane.recording').count()==1;page.get_by_role('button',name='Stop',exact=True).first.click();aligned()
 first=page.locator('.live-item-container').first;clip=first.locator('.audio-clip').first;clip.dispatch_event('contextmenu',{'clientX':700,'clientY':500});assert page.get_by_role('menuitem',name='Выбрать файл…',exact=True).count()==1;page.keyboard.press('Escape')
 for scale in [100,125,150,200]:
  page.locator('#scale').select_option(str(scale));aligned()
 page.locator('#scale').select_option('100');timeline.evaluate("(n)=>n.style.height='350px'");side.evaluate("(n)=>n.style.height='350px'");timeline.evaluate('(n)=>n.scrollTop=10000');page.wait_for_timeout(250);aligned();assert side.evaluate('(n)=>n.scrollTop')>0;last=panel.locator('.shared-channel-title').last;assert last.bounding_box()['y']+last.bounding_box()['height']<=side.bounding_box()['y']+side.bounding_box()['height']+2;page.screenshot(path='/tmp/mr-live-shared-tracks.png');assert not errors,errors;b.close()
print('PASS: one stationary channel column, aligned rows including record/takes at 100/125/150/200, M/mixer sync, global recording, clip file context')
