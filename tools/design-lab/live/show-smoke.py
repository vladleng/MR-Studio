from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=browser.new_page(viewport={'width':1920,'height':1200});errors=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 first=page.locator('[data-id="block-1"]')
 box=first.locator('h3').bounding_box()
 page.mouse.move(box['x']+50,box['y']+8);page.mouse.down();page.mouse.move(box['x']+56,box['y']+8,steps=3);page.mouse.up()
 assert page.locator('[name=x]').input_value()=='32'
 page.locator('#undo').click()
 handle=first.locator('.resize').bounding_box()
 page.mouse.move(handle['x']+8,handle['y']+8);page.mouse.down();page.mouse.move(handle['x']+14,handle['y']+8,steps=3);page.mouse.up()
 assert page.locator('[name=w]').input_value()=='416'
 page.locator('#undo').click()
 page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===8')
 page.locator('#preview').check()
 root=page.locator('.show-browser')
 root.get_by_role('button',name='Browser',exact=True).click()
 root.get_by_role('button',name='+ Папка',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Название',exact=True).fill('Songs');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 root.get_by_label('Папка Browser',exact=True).select_option(label='Songs')
 root.get_by_role('button',name='+ Item',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Название',exact=True).fill('New ballad');dialog.get_by_label('Tempo / BPM').fill('110');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 root.locator('.song-row').filter(has_text='New ballad').click()
 root.get_by_role('button',name='Добавить в setlist',exact=True).click()
 root.get_by_role('button',name='Добавить в setlist',exact=True).click()
 root.get_by_role('button',name='Setlist',exact=True).click()
 assert root.locator('.song-row').count()==5
 root.locator('.song-row').last.click()
 root.get_by_role('button',name='Открыть item',exact=True).click()
 root.get_by_role('button',name='Убрать из setlist',exact=True).click()
 assert root.locator('.song-row').count()==4
 root.get_by_role('button',name='Browser',exact=True).click()
 root.get_by_label('Папка Browser',exact=True).select_option(label='Songs')
 assert root.locator('.tree-group').filter(has=page.get_by_role('button',name='Папка Songs',exact=True)).locator('.song-row').count()==1
 root.locator('.song-row').filter(has_text='New ballad').click();root.get_by_role('button',name='Свойства',exact=True).click()
 dialog=page.get_by_role('dialog');dialog.get_by_label('Название',exact=True).fill('Renamed ballad');dialog.get_by_role('button',name='Сохранить',exact=True).click()
 root.get_by_role('button',name='Setlist',exact=True).click()
 assert 'Renamed ballad' in root.inner_text()
 page.reload();page.wait_for_selector('.block');page.locator('#preview').check()
 root=page.locator('.show-browser');assert 'Renamed ballad' in root.inner_text()
 root.get_by_role('button',name='Browser',exact=True).click()
 root.get_by_label('Папка Browser',exact=True).select_option(label='Songs')
 root.locator('.song-row').filter(has_text='Renamed ballad').click();root.get_by_role('button',name='Удалить item',exact=True).click()
 page.get_by_role('dialog').get_by_role('button',name='Сохранить',exact=True).click()
 assert root.locator('.song-row').filter(has_text='Renamed ballad').count()==0
 root.get_by_role('button',name='Undo show',exact=True).click()
 assert root.locator('.song-row').filter(has_text='Renamed ballad').count()==1
 with page.expect_download() as dl:root.get_by_role('button',name='Экспорт demo show',exact=True).click()
 assert json.load(open(dl.value.path()))['schema']=='mr-live-demo-show'
 assert not errors,errors
 page.screenshot(path='/tmp/mr-live-show-browser.png')
 browser.close()
print('PASS: user layout, Browser/folder/item CRUD, setlist references, rename/reload, delete/Undo, demo export')
