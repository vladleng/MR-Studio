"""Run against a repository-root HTTP server: python smoke.py [base URL]."""
import json, sys
from pathlib import Path
from playwright.sync_api import sync_playwright
base = sys.argv[1] if len(sys.argv)>1 else 'http://127.0.0.1:8000'
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
    page=browser.new_page(viewport={'width':1600,'height':1000})
    errors=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.goto(base+'/tools/design-lab/live/')
    page.wait_for_selector('.block')
    assert page.locator('.block').count()==13
    page.wait_for_function('Array.from(document.querySelectorAll(".visual img")).every(i=>i.complete && i.naturalWidth>0)')
    assert page.locator('.visual-fader').count()==4
    assert page.locator('.visual-wave').count()==5
    assert page.locator('.visual-action').count()==9
    page.locator('#layers button').first.click()
    page.locator('[name=fontSize]').fill('22')
    page.locator('[name=fontSize]').press('Tab')
    assert page.locator('[name=fontSize]').input_value()=='22'
    page.locator('#undo').click()
    assert page.locator('[name=fontSize]').input_value()=='16'
    page.locator('#redo').click()
    page.locator('[name=asset]').select_option(index=1)
    page.wait_for_function('Array.from(document.querySelectorAll(".block img")).every(i=>i.complete && i.naturalWidth>0)')
    page.reload();page.wait_for_selector('.block')
    assert page.locator('.block h3 img').count()==1
    page.locator('#layers button').nth(1).click()
    assert page.locator('.block').count()==12
    page.locator('#layers button').nth(1).click()
    assert page.locator('.block').count()==13
    heading=page.locator('.block h3').first
    box=heading.bounding_box();page.mouse.move(box['x']+60,box['y']+10);page.mouse.down();page.mouse.move(box['x']+84,box['y']+34,steps=4);page.mouse.up()
    assert page.locator('[name=x]').input_value()=='48'
    page.locator('#undo').click()
    assert page.locator('[name=x]').input_value()=='16'
    handle=page.locator('.resize').first;box=handle.bounding_box();page.mouse.move(box['x']+10,box['y']+10);page.mouse.down();page.mouse.move(box['x']+34,box['y']+34,steps=4);page.mouse.up()
    assert page.locator('[name=w]').input_value()=='432'
    page.locator('#undo').click()
    page.locator('#name').fill('Smoke layout');page.locator('#name').press('Tab');page.locator('#save').click()
    with page.expect_download() as download:page.locator('#export').click()
    exported=json.loads(Path(download.value.path()).read_text())
    page.locator('#reset').click()
    page.locator('#import').set_input_files({'name':'roundtrip.json','mimeType':'application/json','buffer':json.dumps(exported).encode()})
    page.wait_for_function('document.getElementById("name").value==="Smoke layout"')
    exported['version']=99
    page.locator('#import').set_input_files({'name':'bad.json','mimeType':'application/json','buffer':json.dumps(exported).encode()})
    page.wait_for_function('document.getElementById("status").textContent.includes("отклонён")')
    assert page.locator('#name').input_value()=='Smoke layout'
    for scale in ['100','125','150','200']:
        page.locator('#scale').select_option(scale)
        for viewport in ['1280,912','1024,768','1920,1080','640,480']:
            page.locator('#viewport').select_option(viewport)
            assert page.locator('.block').count()==13
    page.locator('#scale').select_option('100');page.locator('#viewport').select_option('1280,912')
    for fixture in ['empty','long','many','playing','disabled','selected']:
        page.locator('#fixture').select_option(fixture)
        assert page.locator('.block').count()==13
    page.locator('#fixture').select_option('full')
    page.screenshot(path='/tmp/mr-live-visuals.png')
    page.goto(base+'/tools/design-lab/')
    page.wait_for_selector('#track-stage > *')
    assert page.locator('#asset-grid img').count()>0
    assert not errors,errors
    browser.close()
print('PASS: HTTP Chromium, geometry gestures, history, SVG, hidden layers, export/import, recovery, viewport/scale, fixtures, Track Header')
