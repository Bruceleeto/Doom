#ifndef __MEGATEXTURE_H__
#define __MEGATEXTURE_H__

// MegaTexture removed — stub header
class idMegaTexture {
public:
	bool	InitFromMegaFile( const char *fileBase ) { return false; }
	void	SetMappingForSurface( const srfTriangles_t *tri ) {}
	void	BindForViewOrigin( const idVec3 origin ) {}
	void	Unbind() {}
	static	void MakeMegaTexture_f( const idCmdArgs &args ) {}
};

#endif
