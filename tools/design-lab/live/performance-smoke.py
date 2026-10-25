from playwright.sync_api import sync_playwright
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();first=page.locator('.live-item-container').first
 first.locator('.live-item-header select').select_option('loop');first.locator('.song-part').first.click(button='right');page.get_by_role('menuitem',name='↻ Зациклить',exact=True).click();page.get_by_role('button',name='View record tracks',exact=True).click()
 first.get_by_role('button',name='Запись: Keys',exact=True).click();rec=page.locator('.visual-control-transport .global-record');rec.click();page.wait_for_timeout(350);rec.click()
 row=first.locator('.record-track').first;row.locator('summary').click();assert row.locator('.take-row').count()==1
 row.locator('.take-row input').check();assert 'Comp:' in row.inner_text();row.get_by_role('button',name='Ещё фрагмент',exact=True).click();assert row.locator('.take-row').count()==2
 row.locator('.take-row').last.get_by_role('button',name='Фрагмент…',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Начало',exact=True).fill('4');modal.get_by_label('Конец',exact=True).fill('8');modal.get_by_role('button',name='Сохранить',exact=True).click();row.locator('.take-row input').last.check();assert '4–8' in row.inner_text()
 page.reload();page.wait_for_selector('.block');assert page.locator('.live-item-container').first.locator('.live-item-header select').input_value()=='loop';assert not errors,errors;b.close()
print('PASS: item/section modes, takes creation/spoiler, fragment editing and comp selection, reload')
