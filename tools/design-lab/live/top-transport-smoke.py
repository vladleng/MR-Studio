from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click();assert page.locator('.visual-control-transport').count()==0
 top=page.locator('.timeline-top');controls=top.locator('.timeline-transport');t=top.bounding_box();c=controls.bounding_box();assert abs((t['x']+t['width']/2)-(c['x']+c['width']/2))<2
 assert top.locator('.timeline-tuner').evaluate('(n)=>n.complete&&n.naturalWidth>0');assert top.locator('.timeline-tuner').bounding_box()['width']>150;assert top.bounding_box()['y']<first.bounding_box()['y']
 top.get_by_role('button',name='Следующая секция',exact=True).click();page.wait_for_timeout(120);assert float(first.locator('[data-playhead]').evaluate('(n)=>n.style.left.replace("px","")'))>300
 top.get_by_role('button',name='Предыдущая секция',exact=True).click();top.get_by_role('button',name='Следующий item',exact=True).click();assert doc()['currentItem']==doc()['items'][1]['id'];top.get_by_role('button',name='Предыдущий item',exact=True).click();assert doc()['currentItem']==doc()['items'][0]['id']
 # All chord operations and colors are present in RMB.
 first.locator('.chord-lane').dblclick(position={'x':24,'y':12});chord=first.locator('.chord-event');chord.click(button='right');assert page.get_by_role('menuitem',name='Редактировать аккорд',exact=True).count()==1;assert page.get_by_role('menuitem',name='Дублировать аккорд',exact=True).count()==1;assert page.get_by_role('menuitem',name='Удалить аккорд',exact=True).count()==1;page.get_by_role('menuitem',name='Цвет: Зелёный',exact=True).click();assert doc()['performance']['chords'][doc()['items'][0]['id']][0]['color']=='green'
 # Simulate a dragover to inspect the insertion hint before dropping.
 rows=page.locator('.block .show-browser .show-rows');source=rows.locator('.song-row').first;target=rows.locator('.song-row').last;entry=source.get_attribute('data-entry');iid=source.get_attribute('data-item');data=page.evaluate_handle('()=>new DataTransfer()');page.evaluate('([dt,item,entry])=>{dt.setData("text/mr-show-item",item);dt.setData("text/mr-show-entry",entry)}',[data,iid,entry]);box=target.bounding_box();target.dispatch_event('dragover',{'dataTransfer':data,'clientY':box['y']+box['height']-2});assert rows.locator('.drop-after').count()==1;target.dispatch_event('drop',{'dataTransfer':data});assert doc()['setlists'][0]['entries'][-1]['id']==entry;assert rows.locator('.drop-after').count()==0
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert not errors,errors;page.screenshot(path='/tmp/mr-live-top-transport.png');b.close()
print('PASS: centered timeline transport/tuner, section/item arrows, chord context/color, insertion hint matches reorder, reload')
