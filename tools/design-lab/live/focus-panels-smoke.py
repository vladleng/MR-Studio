from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click();setlist=page.locator('.block .show-browser');base=setlist.locator('..').bounding_box()['height'];toolbar=page.locator('.visual-control-toolbar')
 toolbar.get_by_role('button',name='Playback / stems',exact=True).click();assert page.locator('.visual-playback-stems').count()==0;assert setlist.locator('..').bounding_box()['height']>base
 toolbar.get_by_role('button',name='Status',exact=True).click();assert page.locator('.visual-system-status').count()==0;assert setlist.locator('..').bounding_box()['height']>base+150
 row=setlist.locator('.song-row').nth(1);iid=row.get_attribute('data-item');row.click();page.wait_for_timeout(550);assert doc()['currentItem']==iid;assert page.locator('.live-item-container.current-item').get_attribute('data-item')==iid
 row.dblclick();name=row.get_by_role('textbox',name='Название item',exact=True);name.fill('Renamed inline');name.press('Enter');assert next(i for i in doc()['items'] if i['id']==iid)['name']=='Renamed inline'
 setlist.get_by_role('button',name='Цвет item: Renamed inline',exact=True).click();page.get_by_role('menuitem',name='teal',exact=True).click();assert doc()['performance']['itemColors'][iid]=='teal';assert page.locator(f'.live-item-container[data-item="{iid}"]').first.evaluate('(n)=>n.style.getPropertyValue("--item-color")')=='#65abae'
 # Choose the first item and add a sub-patch and a chord at origin.
 setlist.locator('.song-row').first.click();page.wait_for_timeout(550);first=page.locator('.live-item-container').first;first.locator('.track-patch').first.get_by_role('button',name='+ Sub-patch',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_role('button',name='Сохранить',exact=True).click();first.locator('.chord-lane').dblclick(position={'x':24,'y':12})
 ruler=first.locator('.item-ruler');ruler.click(position={'x':35,'y':8});bar=page.get_by_role('navigation',name='Меню MR Live');bar.get_by_role('button',name='Transport',exact=True).click();page.get_by_role('menuitem',name='Back to start',exact=True).click()
 page.locator('[data-action="transport.play"]').click();page.wait_for_timeout(250);assert first.locator('.song-part.playhead-active').count()==1;assert first.locator('.temporal-subpatch.playhead-active').count()==1
 # The added chord starts near 1.5 bars; seek to it during playback and verify current highlighting.
 chord=first.locator('.chord-event');start=float(chord.get_attribute('data-start'));width=ruler.bounding_box()['width'];bars=doc()['items'][0]['bars'];ruler.click(position={'x':(start+.1)/bars*width,'y':8});page.wait_for_timeout(150);assert first.locator('.current-chord').count()==1
 page.locator('[data-action="transport.stop"]').click();origin=first.locator('[data-playhead]').evaluate('(n)=>parseFloat(n.style.left)');page.wait_for_timeout(150);origin=first.locator('[data-playhead]').evaluate('(n)=>parseFloat(n.style.left)');assert 180<origin<200,origin
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert not errors,errors;b.close()
print('PASS: toolbar panels/setlist reflow, single focus/double inline rename, linked item colors, playhead highlights and back-to-start/reload')
