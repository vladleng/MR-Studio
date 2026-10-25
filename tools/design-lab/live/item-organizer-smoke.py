from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();page.locator('.live-item-container').first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 current=doc()['currentItem'];root=page.locator('.block .show-browser');row=root.locator('.song-row[data-item]').nth(1);row.click(button='right');card=page.locator('.item-menu-card');card.get_by_role('button',name='Создать структуру:',exact=False).click();assert card.locator('.organizer-row').count()==7
 card.get_by_label('Статус задачи Drums',exact=True).select_option('Done');assert card.get_by_role('progressbar',name='Progress item',exact=True).get_attribute('aria-valuenow')=='13';assert row.get_by_role('progressbar').get_attribute('aria-valuenow')=='13';assert row.locator('.item-save-dot').get_attribute('data-dirty')=='false';assert doc()['currentItem']==current
 card.get_by_role('button',name='Свернуть Рыба',exact=True).click();assert card.get_by_label('Статус задачи Drums',exact=True).count()==0;card.get_by_role('button',name='Раскрыть Рыба',exact=True).click()
 card.get_by_role('button',name='Добавить подзадачу: Bass',exact=True).click();assert card.locator('.organizer-row').count()==8;field=card.get_by_label('Название задачи New sub-item',exact=True);field.fill('Bass edit');field.press('Tab');assert card.get_by_label('Название задачи Bass edit',exact=True).count()==1
 card.get_by_role('button',name='Удалить задачу Bass',exact=True).click();assert card.locator('.organizer-row').count()==6
 card.get_by_label('Статус',exact=True).select_option('Wip');assert row.locator('.item-status-chip').inner_text()=='Wip';card.locator('.item-organizer').evaluate('(n)=>n.scrollTop=0');page.screenshot(path='/tmp/live-organizer.png');page.keyboard.press('Escape');card.wait_for(state='hidden')
 page.keyboard.press('Control+z');assert len(doc()['performance']['itemTasks'][doc()['items'][1]['id']])==5
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;page.locator('#preview').check();row.click(button='right');assert page.locator('.organizer-row').count()==6;assert not errors,errors;b.close()
print('PASS: nested organizer template/add/edit/delete/collapse, shared status/progress, passive editing, clean save dot, undo/reload')
