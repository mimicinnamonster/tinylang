// Original monolithic rogue demo for comparison.
// See rogue.tl for the refactored, well-structured version.
//
// Original author's code — kept as reference.

fun println(x="") { print(x) print("\n") }
fun randint(lo=0, hi=1) { ret floor(rand(lo, hi)) }
fun mymin(a=0, b=0) { if a < b { ret a } ret b }
fun dist2(x1=0, y1=0, x2=0, y2=0) { dx=x2-x1; dy=y2-y1; ret dx*dx+dy*dy }
fun mon_name(mt=0) { if mt==1{ret"goblin"} if mt==2{ret"troll"} if mt==3{ret"dragon"} ret"monster" }
fun mon_char(mt=0) { if mt==1{ret"g"} if mt==2{ret"T"} if mt==3{ret"D"} ret"?" }
fun calc_damage(atk=0, ddef=0) { base=atk-ddef; if base<1{base=1} ret base+randint(0,3) }

fun gen_level(lvl=0) {
    MW=60; MH=20; map=[]; y=0
    for y<MH{row=[];x=0;for x<MW{row=row+[0];x=x+1};map=map+[row];y=y+1}
    vis=[]; y=0
    for y<MH{row=[];x=0;for x<MW{row=row+[0];x=x+1};vis=vis+[row];y=y+1}
    rooms=[];n=0;max=randint(6,10);t=0;tm=max*15
    for t<tm&&n<max{
        rw=randint(4,10);rh=randint(3,7);rx=randint(1,MW-rw-1);ry=randint(1,MH-rh-1);ov=0;i=0
        for i<n{rr=rooms[i];if rx<rr[0]+rr[2]+1&&rx+rw+1>rr[0]&&ry<rr[1]+rr[3]+1&&ry+rh+1>rr[1]{ov=1};i=i+1}
        if!ov{rooms=rooms+[[rx,ry,rw,rh]];n=n+1};t=t+1}
    if n==0{rooms=rooms+[[1,1,8,6]];n=1}
    i=0;for i<n{r=rooms[i];rx=r[0];ry=r[1];rw=r[2];rh=r[3];yy=ry;for yy<ry+rh{xx=rx;for xx<rx+rw{map[yy][xx]=1;xx=xx+1};yy=yy+1};i=i+1}
    i=1;for i<n{r1=rooms[i-1];r2=rooms[i];x1=r1[0]+floor(r1[2]/2);y1=r1[1]+floor(r1[3]/2);x2=r2[0]+floor(r2[2]/2);y2=r2[1]+floor(r2[3]/2)
        cx=x1;if x1<=x2{for cx<=x2{if y1>=0&&y1<MH&&cx>=0&&cx<MW{map[y1][cx]=1}cx=cx+1}}else{for cx>=x2{if y1>=0&&y1<MH&&cx>=0&&cx<MW{map[y1][cx]=1}cx=cx-1}}
        cy=y1;if y1<=y2{for cy<=y2{if cy>=0&&cy<MH&&x2>=0&&x2<MW{map[cy][x2]=1}cy=cy+1}}else{for cy>=y2{if cy>=0&&cy<MH&&x2>=0&&x2<MW{map[cy][x2]=1}cy=cy-1}}
        i=i+1}
    r0=rooms[0];sx=r0[0]+floor(r0[2]/2);sy=r0[1]+floor(r0[3]/2)
    rl=rooms[n-1];rsx=rl[0]+floor(rl[2]/2);rsy=rl[1]+floor(rl[3]/2);map[rsy][rsx]=2
    mon=[];i=1;for i<n{r=rooms[i];cnt=randint(1,mymin(3,lvl+1));j=0
        for j<cnt{mx=randint(r[0],r[0]+r[2]-1);my=randint(r[1],r[1]+r[3]-1)
            if!(mx==sx&&my==sy){
                mt=1;rr=randint(1,100);if lvl>=2&&rr>70{mt=2};if lvl>=3&&rr>90{mt=3}
                mhp=8;ma=3;md=1;if mt==2{mhp=20;ma=7;md=3};if mt==3{mhp=40;ma=12;md=5}
                mhp=mhp+(lvl-1)*3;ma=ma+(lvl-1);md=md+(lvl-1)/2
                mon=mon+[[mx,my,mhp,mhp,ma,md,mt,1]]};j=j+1};i=i+1}
    its=[];i=1;for i<n{r=rooms[i];cnt=randint(0,2);j=0
        for j<cnt{ix=randint(r[0],r[0]+r[2]-1);iy=randint(r[1],r[1]+r[3]-1)
            if randint(0,1)==0{hval=randint(5,15);its=its+[[ix,iy,1,hval]]}else{gval=randint(3,12);its=its+[[ix,iy,2,gval]]}
            j=j+1};i=i+1}
    ret[map,vis,mon,its,sx,sy,n]
}

g = gen_level(1)
map = g[0]; visible = g[1]; monsters = g[2]; items = g[3]
px = g[4]; py = g[5]
php = 30; pmhp = 30; patk = 5; pdef = 2; pgold = 0; plevel = 1; pkills = 0
quit = 0
msg_max = 6
messages = ["You enter the dungeon...", "Find > stairs to go deeper."]

radius = 7; fov_rr = radius * radius
vy = py - radius
for vy <= py + radius {
    vx = px - radius
    for vx <= px + radius {
        if vy >= 0 && vy < 20 && vx >= 0 && vx < 60 {
            if dist2(vx, vy, px, py) <= fov_rr { visible[vy][vx] = 1 }
        }
        vx = vx + 1
    }
    vy = vy + 1
}

print("\x1b[?25l")

for !quit {
    print("\x1b[H")
    y = 0
    for y < 20 {
        x = 0
        for x < 60 {
            ch = " "
            if x == px && y == py { ch = "@" } elif visible[y][x] {
                mi = -1; i = 0
                for i < #monsters { m = monsters[i]; if m[7] && m[0]==x && m[1]==y { mi = i }; i = i + 1 }
                if mi >= 0 { ch = mon_char(monsters[mi][6]) } else {
                    ii = -1; i = 0
                    for i < #items { it = items[i]; if it[0]==x && it[1]==y { ii = i }; i = i + 1 }
                    if ii >= 0 { if items[ii][2]==1 { ch = "!" } else { ch = "$" } } else {
                        t = map[y][x]
                        if t == 0 { ch = "#" } elif t == 1 { ch = "." } elif t == 2 { ch = ">" }
                    }
                }
            }
            print(ch)
            x = x + 1
        }
        print("\n")
        y = y + 1
    }

    print("HP: "); print(php); print("/"); print(pmhp)
    print(" ATK:"); print(patk); print(" DEF:"); print(pdef); print(" G:"); print(pgold)
    print(" Lv:"); print(plevel); print(" K:"); print(pkills)
    print("\n")

    if #messages > msg_max { messages = messages[#messages - msg_max:] }
    i = 0
    for i < msg_max {
        idx = #messages - msg_max + i
        if idx >= 0 && idx < #messages { println(messages[idx]) } else { print("\n") }
        i = i + 1
    }

    k = key()
    k0 = 0
    if #k > 0 { k0 = k[0] }
    if k0 == 113 || k0 == 81 || k0 == 3 { quit = 1 }

    dx = 0; dy = 0
    if k0 == 119 { dy = -1 } elif k0 == 115 { dy = 1 } elif k0 == 97 { dx = -1 } elif k0 == 100 { dx = 1 }
    if k0 == 104 { dx = -1 } elif k0 == 106 { dy = 1 } elif k0 == 107 { dy = -1 } elif k0 == 108 { dx = 1 }

    waited = 0; moved = 0; fought = 0
    if k0 == 46 { waited = 1 }
    if dx != 0 || dy != 0 {
        nx = px + dx; ny = py + dy
        if nx >= 0 && nx < 60 && ny >= 0 && ny < 20 {
            if map[ny][nx] != 0 {
                mi = -1; i = 0
                for i < #monsters { m = monsters[i]; if m[7] && m[0]==nx && m[1]==ny { mi=i }; i=i+1 }
                if mi >= 0 {
                    dmg = calc_damage(patk, monsters[mi][5])
                    monsters[mi][2] = monsters[mi][2] - dmg
                    messages = messages + ["You hit " + mon_name(monsters[mi][6]) + " for " + dmg + " damage!"]
                    if monsters[mi][2] <= 0 {
                        monsters[mi][7] = 0; pkills = pkills + 1
                        gv = randint(1,5); if monsters[mi][6]==2 { gv=randint(3,10) }; if monsters[mi][6]==3 { gv=randint(10,30) }
                        pgold = pgold + gv
                        messages = messages + [mon_name(monsters[mi][6]) + " defeated! (+" + gv + " gold)"]
                    }
                    fought = 1
                } else {
                    px = nx; py = ny; moved = 1
                    vy = py - radius
                    for vy <= py + radius {
                        vx = px - radius
                        for vx <= px + radius {
                            if vy>=0 && vy<20 && vx>=0 && vx<60 {
                                if dist2(vx,vy,px,py) <= fov_rr { visible[vy][vx]=1 }
                            }
                            vx = vx + 1
                        }
                        vy = vy + 1
                    }
                    ii = -1; i = 0
                    for i < #items { it = items[i]; if it[0]==px && it[1]==py { ii=i }; i=i+1 }
                    if ii >= 0 {
                        if items[ii][2]==1 { php = php + items[ii][3]; if php>pmhp { php=pmhp }; messages = messages + ["Health potion!"] } else { pgold = pgold + items[ii][3]; messages = messages + ["Gold!"] }
                        items[ii] = items[#items-1]; items = items[:#items-1]
                    }
                    if map[py][px] == 2 {
                        plevel = plevel + 1; pmhp = pmhp + 5; php = pmhp
                        messages = messages + ["Level " + plevel + "!"]
                        g2 = gen_level(plevel)
                        map = g2[0]; visible = g2[1]; monsters = g2[2]; items = g2[3]; px = g2[4]; py = g2[5]
                        vy = py - radius
                        for vy <= py + radius {
                            vx = px - radius
                            for vx <= px + radius {
                                if vy>=0 && vy<20 && vx>=0 && vx<60 {
                                    if dist2(vx,vy,px,py) <= fov_rr { visible[vy][vx]=1 }
                                }
                                vx = vx + 1
                            }
                            vy = vy + 1
                        }
                    }
                }
            }
        }
    }

    if moved || fought || waited {
        i = 0
        for i < #monsters {
            if monsters[i][7] != 0 {
                mx = monsters[i][0]; my = monsters[i][1]; m_atk = monsters[i][4]
                if dist2(mx, my, px, py) <= 2 {
                    dmg = calc_damage(m_atk, pdef); php = php - dmg
                    messages = messages + [mon_name(monsters[i][6]) + " hits you for " + dmg + "!"]
                    if php <= 0 { php = 0; messages = messages + ["You die!"]; quit = 1 }
                } else {
                    moved_mon = 0
                    if dist2(mx, my, px, py) <= 100 {
                        cmx = monsters[i][0]; cmy = monsters[i][1]
                        cdx = 0; cdy = 0
                        if cmx < px { cdx = 1 } elif cmx > px { cdx = -1 }
                        if cmy < py { cdy = 1 } elif cmy > py { cdy = -1 }
                        if cdx != 0 {
                            cnx = cmx + cdx; cny = cmy
                            if cnx>=0 && cnx<60 && cny>=0 && cny<20 && map[cny][cnx]!=0 {
                                mi2 = -1; j=0
                                for j<#monsters { mm=monsters[j]; if mm[7] && mm[0]==cnx && mm[1]==cny { mi2=j }; j=j+1 }
                                if mi2<0 && !(cnx==px && cny==py) { monsters[i][0]=cnx; moved_mon=1 }
                            }
                        }
                        if !moved_mon && cdy != 0 {
                            cnx = cmx; cny = cmy + cdy
                            if cnx>=0 && cnx<60 && cny>=0 && cny<20 && map[cny][cnx]!=0 {
                                mi2 = -1; j=0
                                for j<#monsters { mm=monsters[j]; if mm[7] && mm[0]==cnx && mm[1]==cny { mi2=j }; j=j+1 }
                                if mi2<0 && !(cnx==px && cny==py) { monsters[i][1]=cny; moved_mon=1 }
                            }
                        }
                        if !moved_mon && cdy != 0 {
                            cnx = cmx; cny = cmy + cdy
                            if cnx>=0 && cnx<60 && cny>=0 && cny<20 && map[cny][cnx]!=0 {
                                mi2 = -1; j=0
                                for j<#monsters { mm=monsters[j]; if mm[7] && mm[0]==cnx && mm[1]==cny { mi2=j }; j=j+1 }
                                if mi2<0 && !(cnx==px && cny==py) { monsters[i][1]=cny; moved_mon=1 }
                            }
                        }
                        if !moved_mon && cdx != 0 {
                            cnx = cmx + cdx; cny = cmy
                            if cnx>=0 && cnx<60 && cny>=0 && cny<20 && map[cny][cnx]!=0 {
                                mi2 = -1; j=0
                                for j<#monsters { mm=monsters[j]; if mm[7] && mm[0]==cnx && mm[1]==cny { mi2=j }; j=j+1 }
                                if mi2<0 && !(cnx==px && cny==py) { monsters[i][0]=cnx; moved_mon=1 }
                            }
                        }
                    }
                    if !moved_mon && randint(0,100) < 30 {
                        rd = randint(0,3)
                        rdx = 0; rdy = 0
                        if rd==0 { rdy=-1 } elif rd==1 { rdy=1 } elif rd==2 { rdx=-1 } else { rdx=1 }
                        rnx = monsters[i][0] + rdx; rny = monsters[i][1] + rdy
                        if rnx>=0 && rnx<60 && rny>=0 && rny<20 && map[rny][rnx]!=0 {
                            mi2 = -1; j=0
                            for j<#monsters { mm=monsters[j]; if mm[7] && mm[0]==rnx && mm[1]==rny { mi2=j }; j=j+1 }
                            if mi2<0 && !(rnx==px && rny==py) { monsters[i][0]=rnx; monsters[i][1]=rny }
                        }
                    }
                }
            }
            i = i + 1
        }
    }
}

print("\x1b[?25h")

println("================================")
println("           GAME OVER")
println("================================")
print("Level: "); print(plevel); print("\n")
print("Gold: "); print(pgold); print("\n")
print("Kills: "); print(pkills); print("\n")
println("Thanks for playing!")
