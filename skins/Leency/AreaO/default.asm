include 'me_skin.inc'

SKIN_PARAMS \
  height          = bmp_center1.height,\     ; skin height
  margins         = [5:1:43:1],\          ; margins [left:top:right:bottom]
  colors active   = [binner=0xE1E1E1:\    ; border inner color
                     bouter=0x3E3635:\    ; border outer color
                     bframe=0xE1E1E1],\   ; border frame color
  colors inactive = [binner=0xE1E1E1:\    ; border inner color
                     bouter=0x3E3635:\    ; border outer color
                     bframe=0xE1E1E1],\   ; border frame color
  dtp             = 'default.dtp'          ; dtp colors

SKIN_BUTTONS \
  close    = [-18:5][12:12],\             ; buttons coordinates
  minimize = [-38:5][12:12]               ; [left:top][width:height]

SKIN_BITMAPS \
  left active   = bmp_left1,\              ; skin bitmaps pointers
  left inactive = bmp_left1,\
  oper active   = bmp_right1,\
  oper inactive = bmp_right2,\
  base active   = bmp_center1,\
  base inactive = bmp_center1

BITMAP bmp_left1,  'left1.bmp'               ; skin bitmaps
BITMAP bmp_right1, 'right1.bmp'
BITMAP bmp_right2, 'right2.bmp'
BITMAP bmp_center1,'center1.bmp'
