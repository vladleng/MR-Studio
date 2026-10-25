from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 tid=next(t['id'] for t in doc()['items'][0]['tracks'] if t['media']);bars=doc()['items'][0]['bars'];clip=first.locator('.audio-clip').first;clip.click(button='right');assert page.get_by_role('menuitem',name='Разрезать здесь',exact=True).count()==0;page.get_by_role('menuitem',name='✂ Ножницы',exact=True).click()
 clip.click(position={'x':clip.bounding_box()['width']*8/bars,'y':40});assert first.locator('.audio-clip').count()==2
 second=first.locator('.audio-clip').last;box=second.bounding_box();length=doc()['performance']['clips'][tid][-1]['length'];second.click(position={'x':box['width']*4/length,'y':40});assert first.locator('.audio-clip').count()==3
 # Edge trimming works even while scissors stays selected; it never cuts on release.
 clip=first.locator('.audio-clip').first;grip=clip.locator('.extent-right');grip.hover();assert 'data:image/svg+xml' in grip.evaluate('(n)=>getComputedStyle(n).cursor');assert grip.evaluate('(n)=>getComputedStyle(n,"::after").content')=='"]"'
 box=grip.bounding_box();px=clip.locator('..').bounding_box()['width']/bars;x=box['x']+5;y=box['y']+20;page.mouse.move(x,y);page.mouse.down();page.mouse.move(x-2*px,y,steps=8);page.mouse.up();assert first.locator('.audio-clip').count()==3;assert abs(doc()['performance']['clips'][tid][0]['length']-6)<.26
 clip.click(button='right');page.get_by_role('menuitem',name='↖ Указатель',exact=True).click();box=clip.bounding_box();x=box['x']+box['width']/2;y=box['y']+30;page.mouse.move(x,y);page.mouse.down();page.mouse.move(x+px,y,steps=8);page.mouse.up();assert first.locator('.audio-clip').count()==3;assert abs(doc()['performance']['clips'][tid][0]['start']-1)<.26
 page.keyboard.press('Control+z');assert doc()['performance']['clips'][tid][0]['start']==0;assert not errors,errors;b.close()
print('PASS: persistent scissors selection, repeated grid cuts, bracket cursors/trim in scissors mode, pointer move and Undo')
