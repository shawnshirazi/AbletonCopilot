import csv,json,re,collections,statistics as st
d=json.load(open('tracklists.json'))
eps=d['episodes']
def norm_title(t): return re.sub(r'[^a-z0-9]','',t.lower())
def split_artists(a):
    a=re.sub(r'\s(ft\.|feat\.|pres\.|vs\.|x|with)\s',' & ',a,flags=re.I)
    return [p.strip() for p in re.split(r'&|,',a) if p.strip()]
play=collections.Counter(); artist=collections.Counter(); label=collections.Counter(); names={}
tiesto=[]
for e in eps:
    for t in e['tracks']:
        k=norm_title(split_artists(t['artist'])[0])+'|'+norm_title(t['title']); play[k]+=1; names.setdefault(k,f"{t['artist']} – {t['title']}")
        for a in split_artists(t['artist']): artist[a.upper() if a.lower()=='kellar' else a]+=1
        if t['remix'] and 'Tiësto' in t['remix']: artist['Tiësto (remix/edit)']+=1
        if t['label']: label[t['label']]+=1
        if 'Tiësto' in t['artist'] or (t['remix'] and 'Tiësto' in t['remix']): tiesto.append((e['episode'],t['artist'],t['title'],t['remix']))
print("EPISODES",len(eps),"TRACKS",sum(len(e['tracks']) for e in eps),"full",sum(1 for e in eps if e['coverage'].startswith('full')))
print("\nMOST PLAYED"); [print(v,names[k]) for k,v in play.most_common(45) if v>1]
print("\nTOP ARTISTS"); print(artist.most_common(40))
print("\nLABELS (only where shown)"); print(label.most_common(20))
print("\nTIESTO"); [print(x) for x in tiesto]
print("\nEP GENRE TAGS"); print(collections.Counter(e['episode_genre_tags_1001tl'] for e in eps))
for half,rng in (("001-019",range(1,20)),("020-039",range(20,40))):
    c=collections.Counter(e['episode_genre_tags_1001tl'] for e in eps if e['episode'] in rng); print(half,dict(c))
rows=list(csv.DictReader(open('tracks_features.csv',encoding='utf8')))
use=[r for r in rows if r['version_match']!='no' and r['bpm'] and not r['title'].startswith('Beautiful Places')]
b=[int(r['bpm']) for r in use]
print("\nBPM n",len(b),"mean",round(st.mean(b),1),"median",st.median(b),"min",min(b),"max",max(b))
bins=collections.Counter()
for x in b:
    bins['<=129' if x<=129 else '130-135' if x<=135 else '136-139' if x<=139 else '140-144' if x<=144 else '145-149' if x<=149 else '150+']+=1
print(dict(sorted(bins.items())))
fam=lambda g: 'Trance' if 'Trance' in g else 'Melodic House & Techno' if 'Melodic' in g else 'Techno' if 'Techno' in g else 'Other (house/indie/bass/pop/mainstage/hard dance)'
fc=collections.Counter(fam(r['genre']) for r in use); print(fc)
tr=[int(r['bpm']) for r in use if fam(r['genre'])=='Trance']; print("trance bpm median",st.median(tr),"mean",round(st.mean(tr),1),len(tr))
mh=[int(r['bpm']) for r in use if fam(r['genre'])=='Melodic House & Techno']; print("MHT bpm",st.median(mh),len(mh))
keys=[r['key'] for r in use+[r for r in rows if r['title'].startswith('Beautiful Places')] if r['key']]
mm=collections.Counter('minor' if 'minor' in k else 'major' if 'major' in k else 'unknown' for k in keys); print("KEYS",len(keys),mm)
root=collections.Counter(k.split()[0] for k in keys if 'minor' in k); print("minor roots",root.most_common())
print(collections.Counter(r['genre'] for r in use).most_common())
