from playwright.sync_api import sync_playwright
import json,io,wave
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 d=doc();item=d['items'][0];iid=item['id'];oldbars=item['bars'];old=first.locator('.track-patch').first.bounding_box()['width'];original=first.bounding_box()['width']
 def drag(delta):
  grip=first.get_by_label('Ширина item',exact=True);grip.scroll_into_view_if_needed();box=grip.bounding_box();head=first.locator('.live-item-header').bounding_box();x=box['x']+2;y=head['y']+8;page.mouse.move(x,y);page.mouse.down();page.mouse.move(x+delta,y,steps=8);page.mouse.up()
 drag(60);state=doc()['performance']['arrangement'][iid];assert state['length']>oldbars;assert state['scale']==16;assert abs(first.locator('.track-patch').first.bounding_box()['width']-old)<1
 extended=state['length'];first.get_by_role('button',name='Увеличить item',exact=True).click();state=doc()['performance']['arrangement'][iid];assert state['scale']>16;assert state['length']==extended
 drag(-100);assert doc()['performance']['arrangement'][iid]['length']<extended;assert doc()['items'][0]['sections']==item['sections']
 # A metadata-only WAV longer than item remains a full source clip, not squeezed to item length.
 data=io.BytesIO()
 with wave.open(data,'wb') as w:w.setnchannels(1);w.setsampwidth(1);w.setframerate(8000);w.writeframes(b'\x80'*4000000)
 with page.expect_file_chooser() as chosen:first.locator('.item-track').filter(has=page.locator('.clip-lane')).get_by_role('button',name='Выбрать файл…',exact=True).click()
 chosen.value.set_files({'name':'Long backing.wav','mimeType':'audio/wav','buffer':data.getvalue()})
 page.wait_for_function('JSON.parse(localStorage.getItem("mr-live-demo-show-v1")).items[0].tracks.find(t=>t.media).media.duration>400')
 d=doc();t=next(t for t in d['items'][0]['tracks'] if t['media']);assert d['performance']['clips'][t['id']][0]['length']>extended
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert not errors,errors;b.close()
print('PASS: edge extends/shrinks item without scaling content; +/- only zoom; long WAV duration/clip retained; reload')
