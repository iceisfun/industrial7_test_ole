import re,glob,os,collections,sys
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cnt=collections.Counter(); files=collections.defaultdict(set); ment=set()
tok=re.compile(r'[A-Za-z_]\w*')
for f in glob.glob(root+"/tests/**/*.[ch]",recursive=True):
    sweep="/exports_" in f
    s=open(f,errors="replace").read()
    s2=re.sub(r'/\*.*?\*/','',s,flags=re.S); s2=re.sub(r'//[^\n]*','',s2)
    for m in tok.finditer(s2):
        n=m.group(0)
        ment.add(n)
        if not sweep: cnt[n]+=1; files[n].add(f)
pub=[l.strip() for l in open(root+"/tools/public_surface.txt") if l.strip()]
exp=set()
for p in glob.glob(root+"/tools/*_exports.txt"): exp|={l.strip() for l in open(p) if l.strip()}
pubset=set(pub)
# defs in headers: ignore occurrences that are only type/name hits? keep simple
unc=[n for n in pub if cnt[n]==0]
oth=sorted(n for n in exp-pubset if cnt[n]==0)
thin=[(n,cnt[n]) for n in pub if cnt[n] in(1,2)]
print(len(pub),len(unc),len(oth),len(thin))
open(os.path.join(root,"tools","cov_unc.txt"),"w").write(" ".join(unc))
open(os.path.join(root,"tools","cov_oth.txt"),"w").write(" ".join(oth))
open(os.path.join(root,"tools","cov_thin.txt"),"w").write(" ".join(f"{n}:{c}" for n,c in thin))
