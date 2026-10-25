from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();page.locator('.live-item-container').first.get_by_role('button',name='Открыть item',exact=True).click()
 def check():
  rows=page.locator('.block .song-row[data-item]');assert rows.count()==3
  for row in rows.all():
   bar=row.locator('.item-progress').bounding_box();dot=row.locator('.item-save-dot').bounding_box();name=row.locator('.song-item-name').bounding_box();box=row.bounding_box();assert abs(bar['y']+bar['height']/2-dot['y']-dot['height']/2)<.6;assert row.locator('.song-item-name').evaluate('(n)=>n.offsetTop')<12;assert row.evaluate('(n)=>n.offsetHeight')<=62
 check()
 legacy={'height':80,'fields':{'name':{'x':3,'y':30,'w':58,'h':20,'visible':True},'metadata':{'x':3,'y':53,'w':58,'h':16,'visible':True},'progress':{'x':64,'y':29,'w':27,'h':4,'visible':True},'priority':{'x':64,'y':50,'w':9,'h':18,'visible':True},'status':{'x':74,'y':50,'w':24,'h':20,'visible':True},'saved':{'x':94,'y':25,'w':3,'h':8,'visible':True}}}
 page.evaluate('(layout)=>{const k="mr-live-demo-show-v1",d=JSON.parse(localStorage.getItem(k));d.performance.itemCardLayout=layout;localStorage.setItem(k,JSON.stringify(d));}',legacy);page.reload();page.wait_for_selector('.block');page.locator('#preview').check();check()
 for scale in ['125','150','200']:page.locator('#scale').select_option(scale);check()
 page.locator('#scale').select_option('100');page.locator('#fullscreen-preview').click();check();page.screenshot(path='/tmp/live-compact-fixed.png');assert not errors,errors;b.close()
print('PASS: actual default/legacy card heights and progress/dot center alignment, 100/125/150/200 and fullscreen')
