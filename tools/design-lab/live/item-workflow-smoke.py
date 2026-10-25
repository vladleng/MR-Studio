from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();page.locator('.live-item-container').first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 root=page.locator('.block .show-browser');current=doc()['currentItem'];entry=root.locator('.song-entry').nth(1);row=entry.locator('.song-row');status=entry.locator('.item-status-chip');assert page.locator('.item-save-icon').count()==0;assert entry.locator('.item-save-dot').get_attribute('data-dirty')=='false'
 for state in ['Wip','Wait','Done','Todo']:
  row.click(button='right');card=page.locator('.item-menu-card');card.get_by_label('Статус',exact=True).select_option(state);card.get_by_role('button',name='Применить',exact=True).click();assert status.get_attribute('data-status')==state;assert doc()['currentItem']==current
 assert entry.locator('.item-save-dot').get_attribute('data-dirty')=='false'
 row.click(button='right');page.get_by_role('menuitem',name='Priority / Progress…',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Priority',exact=True).select_option('P1');modal.get_by_label('Progress, %',exact=True).fill('35');modal.get_by_role('button',name='Сохранить',exact=True).click();assert entry.locator('.item-priority').inner_text()=='P1';assert entry.get_by_role('progressbar').get_attribute('aria-valuenow')=='35';assert doc()['currentItem']==current
 row.click(button='right');page.get_by_role('menuitem',name='Save for All',exact=True).click();assert entry.locator('.item-save-dot').get_attribute('data-dirty')=='false';assert doc()['currentItem']==current
 root.get_by_role('button',name='Browser',exact=True).click();browser=page.locator('.runtime-items');assert browser.locator('.item-priority[data-priority=P1]').count()==1
 # Inline rename remains passive and turns only the save dot yellow.
 row.dblclick();row.get_by_role('textbox',name='Название item',exact=True).fill('Workflow demo');row.get_by_role('textbox',name='Название item',exact=True).press('Enter');assert doc()['currentItem']==current;assert entry.locator('.item-save-dot').get_attribute('data-dirty')=='true'
 # Browser status/priority edits must not save or overwrite unrelated draft content.
 snapshot=doc()['library'][doc()['items'][1]['id']];libraryrow=browser.locator('.song-row[data-item]').filter(has_text='Fly Me to the Moon');libraryrow.click(button='right');card=page.locator('.item-menu-card');card.get_by_label('Статус',exact=True).select_option('Done');card.get_by_label('Priority',exact=True).select_option('P3');card.get_by_role('button',name='Применить',exact=True).click();assert row.locator('.item-status-chip').inner_text()=='Done';assert row.locator('.item-priority').inner_text()=='P3';assert browser.locator('.item-status-chip[data-status=Done]').count()==1;assert doc()['library'][doc()['items'][1]['id']]==snapshot;assert doc()['items'][1]['name']=='Workflow demo';assert entry.locator('.item-save-dot').get_attribute('data-dirty')=='true'
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert page.locator('.item-save-dot[data-dirty=true]').count()==1
 page.locator('#preview').check();page.locator('#fullscreen-preview').click();page.screenshot(path='/tmp/mr-live-item-status.png');assert not errors,errors;b.close()
print('PASS: status dropdowns without activation, save dots/Save for All, priority/progress, saved Browser version, rename, reload/fullscreen')
