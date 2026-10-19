// Node + sharp (resolved normally or via NODE_PATH); render QA and previews.
// Never modifies individual SVGs. Raster QA stays under local ignored build/.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const crypto = require('node:crypto');
const root = __dirname;
const reportDir = path.resolve(root, '../../../build/validation/svg-concept-pack');
const catalog = JSON.parse(fs.readFileSync(path.join(root, 'catalogs/fader_control_track_utility_v1.json')));

(async () => {
  fs.mkdirSync(reportDir, {recursive: true});
  const results = [];
  for (const asset of catalog.assets) {
    for (const scale of [1, 1.25, 1.5, 2]) {
      const w = Math.round(asset.size[0]*scale);
      const h = Math.round(asset.size[1]*scale);
      const result = await sharp(path.join(root, asset.file)).resize(w, h).ensureAlpha().raw().toBuffer({resolveWithObject: true});
      if (result.info.width !== w || result.info.height !== h) throw Error('Bad dimensions: '+asset.id);
      let painted = 0, opaque = 0;
      for (let i=3; i<result.data.length; i+=4) {
        if (result.data[i]) painted++;
        if (result.data[i] === 255) opaque++;
      }
      if (!painted || opaque === w*h) throw Error('Empty or opaque full canvas: '+asset.id);
      results.push({id: asset.id, scale, width: w, height: h, paintedPixels: painted,
        sha256: crypto.createHash('sha256').update(result.data).digest('hex')});
    }
  }
  for (const name of ['Fader_Control_Overview', 'Track_Utility_Overview']) {
    await sharp(path.join(root, 'previews', name+'.svg')).png().toFile(path.join(root, 'previews', name+'.png'));
  }
  const report = {renderer: 'sharp '+sharp.versions.sharp+' / librsvg '+sharp.versions.rsvg,
    status: 'PASS', assets: catalog.assets.length, scaleSamples: results.length, results,
    limits: 'Software rasterization only; native JUCE integration, input and physical DPI are not tested.'};
  fs.writeFileSync(path.join(reportDir, 'render_report.json'), JSON.stringify(report, null, 2)+'\n');
  console.log(`PASS: ${results.length} SVG render samples at 100/125/150/200%; 2 PNG overviews`);
})().catch(error => {console.error(error); process.exitCode=1;});
