# -*- cmake -*-

project(viewer)

include(00-Common)
include(Boost)
include(DBusGlib)
include(DirectX)
include(ELFIO)
if(FMODEX)
  include(FMODEX)
  set(FMOD OFF)
endif(FMODEX)
if(FMOD)
  include(FMOD)
endif(FMOD)
include(OPENAL)
include(FindOpenGL)
include(Hunspell)
include(JsonCpp)
include(LLAddBuildTest)
include(LLAudio)
include(LLCharacter)
include(LLCommon)
include(LLImage)
include(LLImageJ2COJ)
include(LLInventory)
include(LLMath)
include(LLMessage)
include(LLPhysicsExtensions)
include(LLPlugin)
include(LLPrimitive)
include(LLRender)
include(LLUI)
include(LLVFS)
include(LLWindow)
include(LLXML)
#include(LScript)
include(Linking)
include(NDOF)
include(StateMachine)
include(TemplateCheck)
include(UI)
include(ViewerMiscLibs)
include(GLOD)
include(LLAppearance)

if (WINDOWS)
    include(CopyWinLibs)
endif (WINDOWS)

include_directories(
    ${CMAKE_SOURCE_DIR}/newview
    ${STATEMACHINE_INCLUDE_DIRS}
    ${DBUSGLIB_INCLUDE_DIRS}
    ${HUNSPELL_INCLUDE_DIR}
    ${ELFIO_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIR}
    ${GLOD_INCLUDE_DIRS}
    ${LLAUDIO_INCLUDE_DIRS}
    ${LLCHARACTER_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLINVENTORY_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLPHYSICSEXTENSIONS_INCLUDE_DIRS}
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLPRIMITIVE_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLUI_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${LLAPPEARANCE_INCLUDE_DIRS}
#    ${LSCRIPT_INCLUDE_DIRS}
#    ${LSCRIPT_INCLUDE_DIRS}/lscript_compile
    )

set(viewer_SOURCE_FILES
    NACLantispam.cpp
    aihttpview.cpp
    aoremotectrl.cpp
    ascentfloatercontactgroups.cpp
    ascentkeyword.cpp
    ascentprefschat.cpp
    ascentprefssys.cpp
    ascentprefsvan.cpp
    awavefront.cpp
    chatbar_as_cmdline.cpp
    daeexport.cpp
    floaterao.cpp
    floaterlocalassetbrowse.cpp
    floatervoicelicense.cpp
	generichandlers.cpp
    hbfloatergrouptitles.cpp
    groupchatlistener.cpp
    hippofloaterxml.cpp
    hippogridmanager.cpp
    hippolimits.cpp
    hippopanelgrids.cpp
    importtracker.cpp
    jcfloaterareasearch.cpp
    lffloaterinvpanel.cpp
    lfsimfeaturehandler.cpp
    lggdicdownload.cpp
    lgghunspell_wrapper.cpp
    llaccountingcostmanager.cpp
    llagent.cpp
    llagentaccess.cpp
    llagentcamera.cpp
    llagentdata.cpp
    llagentlanguage.cpp
    llagentpilot.cpp
    llagentui.cpp
    llagentwearables.cpp
    llagentwearablesfetch.cpp
    llanimstatelabels.cpp
    llappearancemgr.cpp
    llappviewer.cpp
    llassetuploadqueue.cpp
    llassetuploadresponders.cpp
    llattachmentsmgr.cpp
    llaudiosourcevo.cpp
    llavataractions.cpp
    llavatarpropertiesprocessor.cpp
    llbox.cpp
    llcallbacklist.cpp
    llcallingcard.cpp
    llcapabilitylistener.cpp
    llcaphttpsender.cpp
    llchatbar.cpp
    llclassifiedinfo.cpp
    llclassifiedstatsresponder.cpp
    llcloud.cpp
    llcolorscheme.cpp
    llcolorswatch.cpp
    llcommandhandler.cpp
    llcommandlineparser.cpp
    llcompilequeue.cpp
    llconfirmationmanager.cpp
    llconsole.cpp
    llcrashlogger.cpp
    llcurrencyuimanager.cpp
    llcylinder.cpp
    lldaycyclemanager.cpp
    lldebugmessagebox.cpp
    lldebugview.cpp
    lldelayedgestureerror.cpp
    lldrawable.cpp
    lldrawpool.cpp
    lldrawpoolalpha.cpp
    lldrawpoolavatar.cpp
    lldrawpoolbump.cpp
    lldrawpoolground.cpp
    lldrawpoolsimple.cpp
    lldrawpoolsky.cpp
    lldrawpoolterrain.cpp
    lldrawpooltree.cpp
    lldrawpoolwater.cpp
    lldrawpoolwlsky.cpp
    lldroptarget.cpp
    lldynamictexture.cpp
    llemote.cpp
    llenvmanager.cpp
    llestateinfomodel.cpp
    lleventinfo.cpp
    lleventnotifier.cpp
    lleventpoll.cpp
    llexternaleditor.cpp
    llface.cpp
    llfasttimerview.cpp
    llfeaturemanager.cpp
    llfirstuse.cpp
    llflexibleobject.cpp
    llfloaterabout.cpp
    llfloateractivespeakers.cpp
    llfloaterauction.cpp
    llfloateravatarinfo.cpp
    llfloateravatarlist.cpp
    llfloateravatarpicker.cpp
    llfloateravatartextures.cpp
    llfloaterbeacons.cpp
    llfloaterblacklist.cpp
    llfloaterbuildoptions.cpp
    llfloaterbulkpermission.cpp
    llfloaterbump.cpp
    llfloaterbuy.cpp
    llfloaterbuycontents.cpp
    llfloaterbuycurrency.cpp
    llfloaterbuyland.cpp
    llfloaterbvhpreview.cpp
    llfloatercamera.cpp
    llfloaterchat.cpp
    llfloaterchatterbox.cpp
    llfloaterclassified.cpp
    llfloatercolorpicker.cpp
    llfloatercustomize.cpp
    llfloaterdaycycle.cpp
    llfloaterdirectory.cpp
    llfloaterdisplayname.cpp
    llfloatereditui.cpp
    llfloaterenvsettings.cpp
    llfloaterevent.cpp
    llfloaterexploreanimations.cpp
    llfloaterexploresounds.cpp
    llfloaterfeed.cpp
    llfloaterfonttest.cpp
    llfloaterfriends.cpp
    llfloatergesture.cpp
    llfloatergodtools.cpp
    llfloatergroupinfo.cpp
    llfloatergroupinvite.cpp
    llfloatergroups.cpp
    llfloaterhandler.cpp
    llfloaterhtmlcurrency.cpp
    llfloaterhtmlsimple.cpp
    llfloaterhud.cpp
    llfloaterimagepreview.cpp
    llfloaterinspect.cpp
    llfloaterjoystick.cpp
    llfloaterlagmeter.cpp
    llfloaterland.cpp
    llfloaterlandholdings.cpp
    llfloaterlandmark.cpp
    llfloatermap.cpp
    llfloatermediasettings.cpp
    llfloatermemleak.cpp
    llfloatermessagelog.cpp
    llfloatermodelpreview.cpp
    llfloatermodeluploadbase.cpp
    llfloatermute.cpp
    llfloaternamedesc.cpp
    llfloaternotificationsconsole.cpp
    llfloaterobjectiminfo.cpp
    llfloateropenobject.cpp
    llfloateroutbox.cpp
    llfloaterparcel.cpp
    llfloaterpathfindingcharacters.cpp
    llfloaterpathfindinglinksets.cpp
    llfloaterpathfindingobjects.cpp
    llfloaterpermissionsmgr.cpp
    llfloaterperms.cpp
    llfloaterpostcard.cpp
    llfloaterpostprocess.cpp
    llfloaterpreference.cpp
    llfloaterproperties.cpp
    llfloaterregiondebugconsole.cpp
    llfloaterregioninfo.cpp
    llfloaterreporter.cpp
    llfloaterscriptdebug.cpp
    llfloaterscriptlimits.cpp
    llfloatersearch.cpp
    llfloatersearchreplace.cpp
    llfloatersellland.cpp
    llfloatersettingsdebug.cpp
    llfloatersnapshot.cpp
    llfloaterstats.cpp
    llfloatertelehub.cpp
    llfloaterteleporthistory.cpp
    llfloatertest.cpp
    llfloatertools.cpp
    llfloatertopobjects.cpp
    llfloatertos.cpp
    llfloaterurldisplay.cpp
    llfloaterurlentry.cpp
    llfloatervoiceeffect.cpp
    llfloaterwater.cpp
    llfloaterwebcontent.cpp
    llfloaterwebprofile.cpp
    llfloaterwhitelistentry.cpp
    llfloaterwindlight.cpp
    llfloaterworldmap.cpp
    llfolderview.cpp
    llfolderviewitem.cpp
    llfollowcam.cpp
    llframestats.cpp
    llframestatview.cpp
    llgesturemgr.cpp
    llgiveinventory.cpp
    llgivemoney.cpp
    llglsandbox.cpp
    llgroupactions.cpp
    llgroupmgr.cpp
    llgroupnotify.cpp
    llhomelocationresponder.cpp
    llhoverview.cpp
    llhudeffect.cpp
    llhudeffectbeam.cpp
    llhudeffectlookat.cpp
    llhudeffectpointat.cpp
    llhudeffecttrail.cpp
    llhudicon.cpp
    llhudmanager.cpp
    llhudnametag.cpp
    llhudobject.cpp
    llhudrender.cpp
    llhudtext.cpp
    llhudview.cpp
    llimpanel.cpp
    llimview.cpp
    llinventoryactions.cpp
    llinventorybridge.cpp
    llinventoryclipboard.cpp
    llinventoryfilter.cpp
    llinventoryfunctions.cpp
    llinventoryicon.cpp
    llinventorymodel.cpp
    llinventorymodelbackgroundfetch.cpp
    llinventoryobserver.cpp
    llinventorypanel.cpp
    lljoystickbutton.cpp
    lllandmarklist.cpp
    lllogchat.cpp
    llloginhandler.cpp
    llmainlooprepeater.cpp
    llmakeoutfitdialog.cpp
    llmanip.cpp
    llmaniprotate.cpp
    llmanipscale.cpp
    llmaniptranslate.cpp
    llmapresponders.cpp
    llmarketplacefunctions.cpp
    llmarketplacenotifications.cpp
    llmaterialmgr.cpp
    llmediactrl.cpp
    llmediadataclient.cpp
    llmediaremotectrl.cpp
    llmenucommands.cpp
    llmenuoptionpathfindingrebakenavmesh.cpp
    llmeshrepository.cpp
    llmimetypes.cpp
    llmorphview.cpp
    llmoveview.cpp
    llmutelist.cpp
    llnamebox.cpp
    llnameeditor.cpp
    llnamelistctrl.cpp
    llnetmap.cpp
    llnotify.cpp
    lloutfitobserver.cpp
    lloverlaybar.cpp
    llpanelaudioprefs.cpp
    llpanelaudiovolume.cpp
    llpanelavatar.cpp
    llpanelclassified.cpp
    llpanelcontents.cpp
    llpaneldirbrowser.cpp
    llpaneldirclassified.cpp
    llpaneldirevents.cpp
    llpaneldirfind.cpp
    llpaneldirgroups.cpp
    llpaneldirland.cpp
    llpaneldirpeople.cpp
    llpaneldirplaces.cpp
    llpaneldirpopular.cpp
    llpaneldisplay.cpp
    llpaneleditwearable.cpp
    llpanelevent.cpp
    llpanelface.cpp
    llpanelgeneral.cpp
    llpanelgroup.cpp
    llpanelgroupgeneral.cpp
    llpanelgroupinvite.cpp
    llpanelgrouplandmoney.cpp
    llpanelgroupnotices.cpp
    llpanelgrouproles.cpp
    llpanelgroupvoting.cpp
    llpanelinput.cpp
    llpanelland.cpp
    llpanellandaudio.cpp
    llpanellandmedia.cpp
    llpanellandobjects.cpp
    llpanellandoptions.cpp
    llpanellogin.cpp
    llpanelmaininventory.cpp
    llpanelmarketplaceoutboxinventory.cpp
    llpanelmediasettingsgeneral.cpp
    llpanelmediasettingspermissions.cpp
    llpanelmediasettingssecurity.cpp
    llpanelnearbymedia.cpp
    llpanelmorph.cpp
    llpanelmsgs.cpp
    llpanelnetwork.cpp
    llpanelobject.cpp
    llpanelobjectinventory.cpp
    llpanelpermissions.cpp
    llpanelpick.cpp
    llpanelplace.cpp
    llpanelprimmediacontrols.cpp
    llpanelprofile.cpp
    llpanelskins.cpp
    llpanelvoicedevicesettings.cpp
    llpanelvoiceeffect.cpp
    llpanelvolume.cpp
    llpanelweb.cpp
    llparcelselection.cpp
    llparticipantlist.cpp
    llpatchvertexarray.cpp
    llpathfindingcharacter.cpp
    llpathfindingcharacterlist.cpp
    llpathfindinglinkset.cpp
    llpathfindinglinksetlist.cpp
    llpathfindingmanager.cpp
    llpathfindingnavmesh.cpp
    llpathfindingnavmeshstatus.cpp
    llpathfindingobject.cpp
    llpathfindingobjectlist.cpp
    llphysicsmotion.cpp
    llphysicsshapebuilderutil.cpp
    llprefschat.cpp
    llprefsim.cpp
    llprefsvoice.cpp
    llpreview.cpp
    llpreviewanim.cpp
    llpreviewgesture.cpp
    llpreviewlandmark.cpp
    llpreviewnotecard.cpp
    llpreviewscript.cpp
    llpreviewsound.cpp
    llpreviewtexture.cpp
    llproductinforequest.cpp
    llprogressview.cpp
    llregioninfomodel.cpp
    llregionposition.cpp
    llremoteparcelrequest.cpp
    llsavedlogins.cpp
    llsavedsettingsglue.cpp
    llscrollingpanelparam.cpp
    llscrollingpanelparambase.cpp
    llselectmgr.cpp
    llsky.cpp
    llslurl.cpp
    llspatialpartition.cpp
    llspeakers.cpp
    llsprite.cpp
    llstartup.cpp
    llstatusbar.cpp
    llstylemap.cpp
    llsurface.cpp
    llsurfacepatch.cpp
    lltexturecache.cpp
    lltexturectrl.cpp
    lltexturefetch.cpp
    lltextureinfo.cpp
    lltextureinfodetails.cpp
    lltexturestats.cpp
    lltexturestatsuploader.cpp
    lltextureview.cpp
    lltool.cpp
    lltoolbar.cpp
    lltoolbrush.cpp
    lltoolcomp.cpp
    lltooldraganddrop.cpp
    lltoolface.cpp
    lltoolfocus.cpp
    lltoolgrab.cpp
    lltoolgun.cpp
    lltoolindividual.cpp
    lltoolmgr.cpp
    lltoolmorph.cpp
    lltoolobjpicker.cpp
    lltoolpie.cpp
    lltoolpipette.cpp
    lltoolplacer.cpp
    lltoolselect.cpp
    lltoolselectland.cpp
    lltoolselectrect.cpp
    lltoolview.cpp
    lltracker.cpp
    lluploaddialog.cpp
    lluploadfloaterobservers.cpp
    llurl.cpp
    llurldispatcher.cpp
    llurlhistory.cpp
    llurlwhitelist.cpp
    lluserauth.cpp
    llvectorperfoptions.cpp
    llvelocitybar.cpp
    llviewchildren.cpp
    llviewerassetstats.cpp
    llviewerassetstorage.cpp
    llviewerassettype.cpp
    llvieweraudio.cpp
    llviewercamera.cpp
    llviewercontrol.cpp
    llviewerdisplay.cpp
    llviewerdisplayname.cpp
    llviewerfoldertype.cpp
    llviewergenericmessage.cpp
    llviewergesture.cpp
    llviewerinventory.cpp
    llviewerjoint.cpp
    llviewerjointattachment.cpp
    llviewerjointmesh.cpp
    llviewerjoystick.cpp
    llviewerkeyboard.cpp
    llviewerlayer.cpp
    llviewermedia.cpp
    llviewermedia_streamingaudio.cpp
    llviewermediafocus.cpp
    llviewermenu.cpp
    llviewermenufile.cpp
    llviewermessage.cpp
    llviewernetwork.cpp
    llviewerobject.cpp
    llviewerobjectbackup.cpp
    llviewerobjectlist.cpp
    llviewerparcelmedia.cpp
    llviewerparcelmediaautoplay.cpp
    llviewerparcelmgr.cpp
    llviewerparceloverlay.cpp
    llviewerpartsim.cpp
    llviewerpartsource.cpp
    llviewerpluginmanager.cpp
    llviewerregion.cpp
    llviewershadermgr.cpp
    llviewerstats.cpp
    llviewerstatsrecorder.cpp
    llviewertexlayer.cpp
    llviewertexteditor.cpp
    llviewertexture.cpp
    llviewertextureanim.cpp
    llviewertexturelist.cpp
    llviewerthrottle.cpp
    llviewerwearable.cpp
    llviewerwindow.cpp
    llvlcomposition.cpp
    llvlmanager.cpp
    llvoavatar.cpp
    llvoavatarself.cpp
    llvocache.cpp
    llvoclouds.cpp
    llvograss.cpp
    llvoground.cpp
    llvoicecallhandler.cpp
    llvoicechannel.cpp
    llvoiceclient.cpp
    llvoiceremotectrl.cpp
    llvoicevisualizer.cpp
    llvoicevivox.cpp
    llvoinventorylistener.cpp
    llvopartgroup.cpp
    llvosky.cpp
    llvosurfacepatch.cpp
    llvotree.cpp
    llvovolume.cpp
    llvowater.cpp
    llvowlsky.cpp
    llwatchdog.cpp
    llwaterparammanager.cpp
    llwaterparamset.cpp
    llwearablelist.cpp
    llweb.cpp
    llwebprofile.cpp
    llwind.cpp
    llwlanimator.cpp
    llwldaycycle.cpp
    llwlhandlers.cpp
    llwlparammanager.cpp
    llwlparamset.cpp
    llworld.cpp
    llworldmap.cpp
    llworldmapmessage.cpp
    llworldmapview.cpp
    llworldmipmap.cpp
    llxmlrpcresponder.cpp
    m7wlinterface.cpp
    noise.cpp
    pipeline.cpp
    qtoolalign.cpp
    rlvcommon.cpp
    rlvextensions.cpp
    rlvfloaterbehaviour.cpp
    rlvhandler.cpp
    rlvhelper.cpp
    rlvinventory.cpp
    rlvlocks.cpp
    rlvui.cpp
    scriptcounter.cpp
    sgmemstat.cpp
    sgversion.cpp
    shcommandhandler.cpp
    shfloatermediaticker.cpp
    slfloatermediafilter.cpp
    wlfPanel_AdvSettings.cpp
    )

# This gets renamed in the packaging step
set(VIEWER_BINARY_NAME "secondlife-bin" CACHE STRING
    "The name of the viewer executable to create.")

set(viewer_HEADER_FILES
    CMakeLists.txt
    ViewerInstall.cmake

    NACLantispam.h
    aihttpview.h
    aoremotectrl.h
    ascentfloatercontactgroups.h
    ascentkeyword.h
    ascentprefschat.h
    ascentprefssys.h
    ascentprefsvan.h
    awavefront.h
    chatbar_as_cmdline.h
    daeexport.h
    floaterao.h
    floaterlocalassetbrowse.h
    floatervoicelicense.h
	generichandlers.h
    groupchatlistener.h
    hbfloatergrouptitles.h
    hippofloaterxml.h
    hippogridmanager.h
    hippolimits.h
    hippopanelgrids.h
    importtracker.h
    jcfloaterareasearch.h
    lffloaterinvpanel.h
    lfsimfeaturehandler.h
    lggdicdownload.h
    lgghunspell_wrapper.h
    llaccountingcostmanager.h
    llagent.h
    llagentaccess.h
    llagentcamera.h
    llagentdata.h
    llagentlanguage.h
    llagentpilot.h
    llagentui.h
    llagentwearables.h
    llagentwearablesfetch.h
    llanimstatelabels.h
    llappearance.h
    llappearancemgr.h
    llappviewer.h
    llassetuploadqueue.h
    llassetuploadresponders.h
    llattachmentsmgr.h
    llaudiosourcevo.h
    llavataractions.h
    llavatarpropertiesprocessor.h
    llbox.h
    llcallbacklist.h
    llcallingcard.h
    llcapabilitylistener.h
    llcaphttpsender.h
    llchatbar.h
    llclassifiedinfo.h
    llclassifiedstatsresponder.h
    llcloud.h
    llcolorscheme.h
    llcolorswatch.h
    llcommandhandler.h
    llcommandlineparser.h
    llcompilequeue.h
    llconfirmationmanager.h
    llconsole.h
    llcrashlogger.h
    llcurrencyuimanager.h
    llcylinder.h
    lldaycyclemanager.h
    lldebugmessagebox.h
    lldebugview.h
    lldelayedgestureerror.h
    lldrawable.h
    lldrawpool.h
    lldrawpoolalpha.h
    lldrawpoolavatar.h
    lldrawpoolbump.h
    lldrawpoolground.h
    lldrawpoolsimple.h
    lldrawpoolsky.h
    lldrawpoolterrain.h
    lldrawpooltree.h
    lldrawpoolwater.h
    lldrawpoolwlsky.h
    lldroptarget.h
    lldynamictexture.h
    llemote.h
    llenvmanager.h
    llestateinfomodel.h
    lleventinfo.h
    lleventnotifier.h
    lleventpoll.h
    llexternaleditor.h
    llface.h
    llfasttimerview.h
    llfeaturemanager.h
    llfirstuse.h
    llflexibleobject.h
    llfloaterabout.h
    llfloateractivespeakers.h
    llfloaterauction.h
    llfloateravatarinfo.h
    llfloateravatarlist.h
    llfloateravatarpicker.h
    llfloateravatartextures.h
    llfloaterbeacons.h
    llfloaterblacklist.h
    llfloaterbuildoptions.h
    llfloaterbulkpermission.h
    llfloaterbump.h
    llfloaterbuy.h
    llfloaterbuycontents.h
    llfloaterbuycurrency.h
    llfloaterbuyland.h
    llfloaterbvhpreview.h
    llfloatercamera.h
    llfloaterchat.h
    llfloaterchatterbox.h
    llfloaterclassified.h
    llfloatercolorpicker.h
    llfloatercustomize.h
    llfloaterdaycycle.h
    llfloaterdirectory.h
    llfloaterdisplayname.h
    llfloatereditui.h
    llfloaterenvsettings.h
    llfloaterevent.h
    llfloaterexploreanimations.h
    llfloaterexploresounds.h
    llfloaterfeed.h
    llfloaterfonttest.h
    llfloaterfriends.h
    llfloatergesture.h
    llfloatergodtools.h
    llfloatergroupinfo.h
    llfloatergroupinvite.h
    llfloatergroups.h
    llfloaterhandler.h
    llfloaterhtmlcurrency.h
    llfloaterhtmlsimple.h
    llfloaterhud.h
    llfloaterimagepreview.h
    llfloaterinspect.h
    llfloaterjoystick.h
    llfloaterlagmeter.h
    llfloaterland.h
    llfloaterlandholdings.h
    llfloaterlandmark.h
    llfloatermap.h
    llfloatermediasettings.h
    llfloatermemleak.h
    llfloatermessagelog.h
    llfloatermodelpreview.h
    llfloatermodeluploadbase.h
    llfloatermute.h
    llfloaternamedesc.h
    llfloaternotificationsconsole.h
    llfloaterobjectiminfo.h
    llfloateropenobject.h
    llfloateroutbox.h
    llfloaterparcel.h
    llfloaterpathfindingcharacters.h
    llfloaterpathfindinglinksets.h
    llfloaterpathfindingobjects.h
    llfloaterpermissionsmgr.h
    llfloaterperms.h
    llfloaterpostcard.h
    llfloaterpostprocess.h
    llfloaterpreference.h
    llfloaterproperties.h
    llfloaterregiondebugconsole.h
    llfloaterregioninfo.h
    llfloaterreporter.h
    llfloaterscriptdebug.h
    llfloaterscriptlimits.h
    llfloatersearch.h
    llfloatersearchreplace.h
    llfloatersellland.h
    llfloatersettingsdebug.h
    llfloatersnapshot.h
    llfloaterstats.h
    llfloatertelehub.h
    llfloaterteleporthistory.h
    llfloatertest.h
    llfloatertools.h
    llfloatertopobjects.h
    llfloatertos.h
    llfloaterurldisplay.h
    llfloaterurlentry.h
    llfloatervoiceeffect.h
    llfloaterwater.h
    llfloaterwebcontent.h
    llfloaterwebprofile.h
    llfloaterwhitelistentry.h
    llfloaterwindlight.h
    llfloaterworldmap.h
    llfolderview.h
    llfoldervieweventlistener.h
    llfolderviewitem.h
    llfollowcam.h
    llframestats.h
    llframestatview.h
    llgesturemgr.h
    llgiveinventory.h
    llgivemoney.h
    llgroupactions.h
    llgroupmgr.h
    llgroupnotify.h
    llhomelocationresponder.h
    llhoverview.h
    llhudeffect.h
    llhudeffectbeam.h
    llhudeffectlookat.h
    llhudeffectpointat.h
    llhudeffecttrail.h
    llhudicon.h
    llhudmanager.h
    llhudnametag.h
    llhudobject.h
    llhudrender.h
    llhudtext.h
    llhudview.h
    llimpanel.h
    llimview.h
    llinventorybridge.h
    llinventoryclipboard.h
    llinventoryfilter.h
    llinventoryfunctions.h
    llinventoryicon.h
    llinventorymodel.h
    llinventorymodelbackgroundfetch.h
    llinventoryobserver.h
    llinventorypanel.h
    lljoystickbutton.h
    lllandmarklist.h
    lllightconstants.h
    lllogchat.h
    llloginhandler.h
    llmainlooprepeater.h
    llmakeoutfitdialog.h
    llmanip.h
    llmaniprotate.h
    llmanipscale.h
    llmaniptranslate.h
    llmapresponders.h
    llmarketplacefunctions.h
    llmarketplacenotifications.h
    llmaterialmgr.h
    llmediactrl.h
    llmediadataclient.h
    llmediaremotectrl.h
    llmenucommands.h
    llmenuoptionpathfindingrebakenavmesh.h
    llmeshrepository.h
    llmimetypes.h
    llmorphview.h
    llmoveview.h
    llmutelist.h
    llnamebox.h
    llnameeditor.h
    llnamelistctrl.h
    llnetmap.h
    llnotify.h
    lloutfitobserver.h
    lloverlaybar.h
    llpanelaudioprefs.h
    llpanelaudiovolume.h
    llpanelavatar.h
    llpanelclassified.h
    llpanelcontents.h
    llpaneldirbrowser.h
    llpaneldirclassified.h
    llpaneldirevents.h
    llpaneldirfind.h
    llpaneldirgroups.h
    llpaneldirland.h
    llpaneldirpeople.h
    llpaneldirplaces.h
    llpaneldirpopular.h
    llpaneldisplay.h
    llpaneleditwearable.h
    llpanelevent.h
    llpanelface.h
    llpanelgeneral.h
    llpanelgroup.h
    llpanelgroupgeneral.h
    llpanelgroupinvite.h
    llpanelgrouplandmoney.h
    llpanelgroupnotices.h
    llpanelgrouproles.h
    llpanelgroupvoting.h
    llpanelinput.h
    llpanelland.h
    llpanellandaudio.h
    llpanellandmedia.h
    llpanellandobjects.h
    llpanellandoptions.h
    llpanellogin.h
    llpanelmaininventory.h
    llpanelmarketplaceoutboxinventory.h
    llpanelmediasettingsgeneral.h
    llpanelmediasettingspermissions.h
    llpanelmediasettingssecurity.h
    llpanelnearbymedia.h
    llpanelmorph.h
    llpanelmsgs.h
    llpanelnetwork.h
    llpanelobject.h
    llpanelobjectinventory.h
    llpanelpermissions.h
    llpanelpick.h
    llpanelplace.h
    llpanelprimmediacontrols.h
    llpanelprofile.h
    llpanelskins.h
    llpanelvoicedevicesettings.h
    llpanelvoiceeffect.h
    llpanelvolume.h
    llpanelweb.h
    llparcelselection.h
    llparticipantlist.h
    llpatchvertexarray.h
    llpathfindingcharacter.h
    llpathfindingcharacterlist.h
    llpathfindinglinkset.h
    llpathfindinglinksetlist.h
    llpathfindingmanager.h
    llpathfindingnavmesh.h
    llpathfindingnavmeshstatus.h
    llpathfindingobject.h
    llpathfindingobjectlist.h
    llphysicsmotion.h
    llphysicsshapebuilderutil.h
    llprefschat.h
    llprefsim.h
    llprefsvoice.h
    llpreview.h
    llpreviewanim.h
    llpreviewgesture.h
    llpreviewlandmark.h
    llpreviewnotecard.h
    llpreviewscript.h
    llpreviewsound.h
    llpreviewtexture.h
    llproductinforequest.h
    llprogressview.h
    llregioninfomodel.h
    llregionposition.h
    llremoteparcelrequest.h
    llresourcedata.h
    llsavedlogins.h
    llsavedsettingsglue.h
    llscrollingpanelparam.h
    llscrollingpanelparambase.h
    llselectmgr.h
    llsimplestat.h
    llsky.h
    llslurl.h
    llspatialpartition.h
    llspeakers.h
    llsprite.h
    llstartup.h
    llstatusbar.h
    llstylemap.h
    llsurface.h
    llsurfacepatch.h
    lltable.h
    lltexturecache.h
    lltexturectrl.h
    lltexturefetch.h
    lltextureinfo.h
    lltextureinfodetails.h
    lltexturestats.h
    lltexturestatsuploader.h
    lltextureview.h
    lltool.h
    lltoolbar.h
    lltoolbrush.h
    lltoolcomp.h
    lltooldraganddrop.h
    lltoolface.h
    lltoolfocus.h
    lltoolgrab.h
    lltoolgun.h
    lltoolindividual.h
    lltoolmgr.h
    lltoolmorph.h
    lltoolobjpicker.h
    lltoolpie.h
    lltoolpipette.h
    lltoolplacer.h
    lltoolselect.h
    lltoolselectland.h
    lltoolselectrect.h
    lltoolview.h
    lltracker.h
    lltranslate.h
    lluiconstants.h
    lluploaddialog.h
    lluploadfloaterobservers.h
    llurl.h
    llurldispatcher.h
    llurlhistory.h
    llurlwhitelist.h
    lluserauth.h
    llvectorperfoptions.h
    llvelocitybar.h
    llviewchildren.h
    llviewerassetstats.h
    llviewerassetstorage.h
    llviewerassettype.h
    llvieweraudio.h
    llviewercamera.h
    llviewercontrol.h
    llviewerdisplay.h
    llviewerdisplayname.h
    llviewerfoldertype.h
    llviewergenericmessage.h
    llviewergesture.h
    llviewerinventory.h
    llviewerjoint.h
    llviewerjointattachment.h
    llviewerjointmesh.h
    llviewerjoystick.h
    llviewerkeyboard.h
    llviewerlayer.h
    llviewermedia.h
    llviewermediafocus.h
    llviewermenu.h
    llviewermenufile.h
    llviewermessage.h
    llviewernetwork.h
    llviewerobject.h
    llviewerobjectbackup.h
    llviewerobjectlist.h
    llviewerparcelmedia.h
    llviewerparcelmediaautoplay.h
    llviewerparcelmgr.h
    llviewerparceloverlay.h
    llviewerpartsim.h
    llviewerpartsource.h
    llviewerpluginmanager.h
    llviewerprecompiledheaders.h
    llviewerregion.h
    llviewershadermgr.h
    llviewerstats.h
    llviewerstatsrecorder.h
    llviewertexlayer.h
    llviewertexteditor.h
    llviewertexture.h
    llviewertextureanim.h
    llviewertexturelist.h
    llviewerthrottle.h
    llviewerwearable.h
    llviewerwindow.h
    llvlcomposition.h
    llvlmanager.h
    llvoavatar.h
    llvoavatarself.h
    llvocache.h
    llvoclouds.h
    llvograss.h
    llvoground.h
    llvoicechannel.h
    llvoiceclient.h
    llvoiceremotectrl.h
    llvoicevisualizer.h
    llvoicevivox.h
    llvoinventorylistener.h
    llvopartgroup.h
    llvosky.h
    llvosurfacepatch.h
    llvotree.h
    llvotreenew.h
    llvovolume.h
    llvowater.h
    llvowlsky.h
    llwatchdog.h
    llwaterparammanager.h
    llwaterparamset.h
    llwearablelist.h
    llweb.h
    llwebprofile.h
    llwind.h
    llwindebug.h
    llwlanimator.h
    llwldaycycle.h
    llwlhandlers.h
    llwlparammanager.h
    llwlparamset.h
    llworld.h
    llworldmap.h
    llworldmapmessage.h
    llworldmapview.h
    llworldmipmap.h
    llxmlrpcresponder.h
    m7wlinterface.h
    macmain.h
    noise.h
    pipeline.h
    qtoolalign.h
    rlvcommon.h
    rlvdefines.h
    rlvextensions.h
    rlvfloaterbehaviour.h
    rlvhandler.h
    rlvhelper.h
    rlvinventory.h
    rlvlocks.h
    rlvui.h
    scriptcounter.h
    sgmemstat.h
    sgversion.h
    shcommandhandler.h
    shfloatermediaticker.h
    slfloatermediafilter.h
    wlfPanel_AdvSettings.h
    VertexCache.h
    VorbisFramework.h
    )

source_group("CMake Rules" FILES ViewerInstall.cmake)

if (DARWIN)
  LIST(APPEND viewer_SOURCE_FILES llappviewermacosx.cpp)

  find_library(AGL_LIBRARY AGL)
  find_library(APPKIT_LIBRARY AppKit)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(IOKIT_LIBRARY IOKit)

  set(viewer_LIBRARIES
    ${COCOA_LIBRARY}
    ${AGL_LIBRARY}
    ${IOKIT_LIBRARY}
    )

  # Add resource files to the project.
  set(viewer_RESOURCE_FILES
    ${VIEWER_BRANDING_ID}.icns
    macview.r
    gpu_table.txt
    Info-${VIEWER_BRANDING_NAME_CAMELCASE}.plist
    SecondLife.nib/
    # CMake doesn't seem to support Xcode language variants well just yet
    English.lproj/InfoPlist.strings
    English.lproj/language.txt
    German.lproj/language.txt
    Japanese.lproj/language.txt
    Korean.lproj/language.txt
    )
  set_source_files_properties(
    ${viewer_RESOURCE_FILES}
    PROPERTIES
    HEADER_FILE_ONLY TRUE
    #MACOSX_PACKAGE_LOCATION Resources #don't do this! this tells cmake to copy the files.
    )
  SOURCE_GROUP("Resources" FILES ${viewer_RESOURCE_FILES})
  list(APPEND viewer_SOURCE_FILES ${viewer_RESOURCE_FILES})
endif (DARWIN)

if (LINUX)
    LIST(APPEND viewer_SOURCE_FILES llappviewerlinux.cpp)
    LIST(APPEND viewer_SOURCE_FILES llappviewerlinux_api_dbus.cpp)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")

    set(viewer_LIBRARIES
        Xinerama
        )
endif (LINUX)

if (WINDOWS)
    list(APPEND viewer_SOURCE_FILES
         llappviewerwin32.cpp
         llwindebug.cpp
         )

    list(APPEND viewer_HEADER_FILES
         llappviewerwin32.h
         llwindebug.h
         )

    # precompiled header configuration
    # llviewerprecompiledheaders.cpp generates
    # the .pch file.
    # All sources added to viewer_SOURCE_FILES
    # at this point use it.
    set_source_files_properties(llviewerprecompiledheaders.cpp
        PROPERTIES
        COMPILE_FLAGS "/Ycllviewerprecompiledheaders.h"
        )
    foreach( src_file ${viewer_SOURCE_FILES} )
        set_source_files_properties(
            ${src_file}
            PROPERTIES
            COMPILE_FLAGS "/Yullviewerprecompiledheaders.h"
            )        
    endforeach( src_file ${viewer_SOURCE_FILES} )
    list(APPEND viewer_SOURCE_FILES llviewerprecompiledheaders.cpp)
    
    # Add resource files to the project.
    # viewerRes.rc is the only buildable file, but
    # the rest are all dependencies of it.
    set(viewer_RESOURCE_FILES
        res/arrow.cur
        res/arrowcop.cur
        res/arrowcopmulti.cur
        res/arrowdrag.cur
        res/circleandline.cur
        res/icon1.ico
        res/llarrow.cur
        res/llarrowdrag.cur
        res/llarrowdragmulti.cur
        res/llarrowlocked.cur
        res/llgrablocked.cur
        res/llno.cur
        res/llnolocked.cur
        res/lltoolcamera.cur
        res/lltoolcreate.cur
        res/lltoolfocus.cur
        res/lltoolgrab.cur
        res/lltoolland.cur
        res/lltoolpan.cur
        res/lltoolpipette.cur
        res/lltoolrotate.cur
        res/lltoolscale.cur
        res/lltooltranslate.cur
        res/lltoolzoomin.cur
        res/lltoolzoomout.cur
        res/snowglobe_icon.BMP
        res/snowglobe_icon.ico
        res/resource.h
        res/toolpickobject.cur
        res/toolpickobject2.cur
        res/toolpickobject3.cur
        res/toolpipette.cur
        res/toolbuy.cur
        res/toolopen.cur
        res/toolpay.cur
        res/toolsit.cur
        )

    set_source_files_properties(${viewer_RESOURCE_FILES}
                                PROPERTIES HEADER_FILE_ONLY TRUE)

    set(viewer_RESOURCE_FILES
       res/resource.h
        res/viewerRes.rc
        ${viewer_RESOURCE_FILES}
        )

    SOURCE_GROUP("Resource Files" FILES ${viewer_RESOURCE_FILES})

    list(APPEND viewer_SOURCE_FILES ${viewer_RESOURCE_FILES})

    find_library(DINPUT_LIBRARY dinput8 ${DIRECTX_LIBRARY_DIR})
    find_library(DXGUID_LIBRARY dxguid ${DIRECTX_LIBRARY_DIR})
    mark_as_advanced(
        DINPUT_LIBRARY
        DXGUID_LIBRARY
        )

# see EXP-1765 - theory is opengl32.lib needs to be included before gdi32.lib (windows libs)
    set(viewer_LIBRARIES
        opengl32
        advapi32
        comdlg32
        ${DINPUT_LIBRARY}
        ${DXGUID_LIBRARY}
        gdi32
        kernel32
        odbc32
        odbccp32
        ole32
        oleaut32
        shell32
        user32
        Vfw32
        winspool
        )

	if(FMODEX)
		list(APPEND viewer_LIBRARIES ${FMODEX_LIBRARY})
	endif(FMODEX)
	if(FMOD)
		list(APPEND viewer_LIBRARIES ${FMOD_LIBRARY})
	endif(FMOD)

    find_library(INTEL_MEMOPS_LIBRARY
                 NAMES ll_intel_memops
                 PATHS
                 optimized ${ARCH_PREBUILT_DIRS_RELEASE}
                 debug ${ARCH_PREBUILT_DIRS_DEBUG}
                 )
    mark_as_advanced(INTEL_MEMOPS_LIBRARY)

    if (INTEL_MEMOPS_LIBRARY)
      list(APPEND viewer_LIBRARIES ${INTEL_MEMOPS_LIBRARY})
    endif (INTEL_MEMOPS_LIBRARY)
endif (WINDOWS)

# Add the xui files. This is handy for searching for xui elements
# from within the IDE.
set(viewer_XUI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/skins/default/colors.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/skins/default/colors_base.xml
    )
file(GLOB viewer_XUI_FILE_GLOB_LIST
     ${CMAKE_CURRENT_SOURCE_DIR}/skins/default/xui/en-us/*.xml)
list(APPEND viewer_XUI_FILES ${viewer_XUI_FILE_GLOB_LIST})



list(SORT viewer_XUI_FILES)

source_group("XUI Files" FILES ${viewer_XUI_FILES})

set_source_files_properties(${viewer_XUI_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND viewer_SOURCE_FILES ${viewer_XUI_FILES})

set(viewer_APPSETTINGS_FILES
    viewer_manifest.py
    app_settings/anim.ini
    app_settings/cmd_line.xml
    app_settings/grass.xml
    app_settings/high_graphics.xml
    app_settings/keys.ini
    app_settings/keywords.ini
    app_settings/logcontrol.xml
    app_settings/low_graphics.xml
    app_settings/mid_graphics.xml
    app_settings/settings.xml
    app_settings/settings_ascent.xml
    app_settings/settings_ascent_coa.xml
    app_settings/settings_crash_behavior.xml
    app_settings/settings_files.xml
    app_settings/settings_per_account.xml
    app_settings/settings_sh.xml
    app_settings/settings_rlv.xml
    app_settings/std_bump.ini
    app_settings/trees.xml
    app_settings/ultra_graphics.xml
    app_settings/viewerart.xml
    ${CMAKE_SOURCE_DIR}/../etc/message.xml
    ${CMAKE_SOURCE_DIR}/../scripts/messages/message_template.msg
    )

source_group("App Settings" FILES ${viewer_APPSETTINGS_FILES})

set_source_files_properties(${viewer_APPSETTINGS_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND viewer_SOURCE_FILES ${viewer_APPSETTINGS_FILES})

set(viewer_CHARACTER_FILES
    character/attentions.xml
    character/attentionsN.xml
    character/avatar_lad.xml
    character/avatar_skeleton.xml
    character/genepool.xml
    )

source_group("Character File" FILES ${viewer_CHARACTER_FILES})

set_source_files_properties(${viewer_CHARACTER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND viewer_SOURCE_FILES ${viewer_CHARACTER_FILES})

if (WINDOWS)
  file(GLOB viewer_INSTALLER_FILES installers/windows/*.nsi)

  source_group("Installer Files" FILES ${viewer_INSTALLER_FILES})

  set_source_files_properties(${viewer_INSTALLER_FILES}
                              PROPERTIES HEADER_FILE_ONLY TRUE)

  list(APPEND viewer_SOURCE_FILES ${viewer_INSTALLER_FILES})
endif (WINDOWS)

if (OPENAL)
  set(LLSTARTUP_COMPILE_FLAGS "${LLSTARTUP_COMPILE_FLAGS} -DLL_OPENAL")
endif (OPENAL)

if (FMOD OR FMODEX)
  if (FMODEX)
    set(LLSTARTUP_COMPILE_FLAGS "${LLSTARTUP_COMPILE_FLAGS} -DLL_FMODEX")
  endif (FMODEX)
  if (FMOD)
    set(LLSTARTUP_COMPILE_FLAGS "${LLSTARTUP_COMPILE_FLAGS} -DLL_FMOD")
  endif (FMOD)

  if (DARWIN AND FMOD)
      set(fmodwrapper_SOURCE_FILES fmodwrapper.cpp)
      add_library(fmodwrapper SHARED ${fmodwrapper_SOURCE_FILES})
      set(fmodwrapper_needed_LIBRARIES ${FMOD_LIBRARY} ${CARBON_LIBRARY})
      set_target_properties(
        fmodwrapper
        PROPERTIES
        BUILD_WITH_INSTALL_RPATH 1
        INSTALL_NAME_DIR "@executable_path/../Resources"
        LINK_FLAGS "-unexported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/fmod_hidden_symbols.exp"
        )
      set(FMODWRAPPER_LIBRARY fmodwrapper)
      target_link_libraries(fmodwrapper ${fmodwrapper_needed_LIBRARIES})
  else (DARWIN AND FMOD)
    # fmodwrapper unnecessary on linux or windows, for fmod and fmodex
    if (FMODEX)
       set(FMODWRAPPER_LIBRARY ${FMODEX_LIBRARY})
    endif (FMODEX)
    if (FMOD)
       set(FMODWRAPPER_LIBRARY ${FMOD_LIBRARY})
    endif (FMOD)
  endif (DARWIN AND FMOD)
endif (FMOD OR FMODEX)

set_source_files_properties(llstartup.cpp PROPERTIES COMPILE_FLAGS "${LLSTARTUP_COMPILE_FLAGS}")

list(APPEND viewer_SOURCE_FILES ${viewer_HEADER_FILES})

set_source_files_properties(${viewer_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

add_executable(${VIEWER_BINARY_NAME}
    WIN32
    MACOSX_BUNDLE
    ${viewer_SOURCE_FILES}
    )

if (!DISABLE_TEMPLATE_CHECK)
	check_message_template(${VIEWER_BINARY_NAME})
endif (!DISABLE_TEMPLATE_CHECK)

# We package by default on Linux so we can run from newview/packaged.
if (LINUX)
  set(PACKAGE_DEFAULT ON)
else (LINUX)
  set(PACKAGE_DEFAULT OFF)
endif (LINUX)
set(PACKAGE ${PACKAGE_DEFAULT} CACHE BOOL
    "Add a package target that builds an installer package.")

if (WINDOWS)
    if(MSVC10)
        set(release_flags "/MAPRelease/${VIEWER_BINARY_NAME}.map")
    else(MSVC10)
        set(release_flags "/MAP:Release/${VIEWER_BINARY_NAME}.map")
    endif(MSVC10)

    if (FMOD)
        if(MANIFEST_LIBRARIES)
            set(MANIFEST_LIBRARIES "${MANIFEST_LIBRARIES}|${FMOD_BINARY_DIR}/fmod.dll")
        else(MANIFEST_LIBRARIES)
            set(MANIFEST_LIBRARIES "--extra_libraries=${FMOD_BINARY_DIR}/fmod.dll")
        endif(MANIFEST_LIBRARIES)
    endif (FMOD)
    if (FMODEX)
        if(MANIFEST_LIBRARIES)
       	    set(MANIFEST_LIBRARIES "${MANIFEST_LIBRARIES}|${FMODEX_BINARY_DIR}/fmodex.dll")
        else(MANIFEST_LIBRARIES)
            set(MANIFEST_LIBRARIES "--extra_libraries=${FMODEX_BINARY_DIR}/fmodex.dll")
        endif(MANIFEST_LIBRARIES)
        set(EXTRA_LINKER_FLAGS "/DELAYLOAD:fmodex.dll")
    endif (FMODEX)

    set_target_properties(${VIEWER_BINARY_NAME}
        PROPERTIES
        LINK_FLAGS "/debug /NODEFAULTLIB:LIBCMT /SUBSYSTEM:WINDOWS ${GOOGLE_PERFTOOLS_LINKER_FLAGS} ${EXTRA_LINKER_FLAGS}"
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:\"LIBCMT;LIBCMTD;MSVCRT\""
        LINK_FLAGS_RELEASE ${release_flags}
        )

    # sets the 'working directory' for debugging from visual studio.
    if (NOT UNATTENDED)
        add_custom_command(
            TARGET ${VIEWER_BINARY_NAME} PRE_BUILD
            COMMAND ${CMAKE_SOURCE_DIR}/tools/vstool/vstool.exe
            ARGS
              --solution
              ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln
              --workingdir
              ${VIEWER_BINARY_NAME}
              "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Setting the ${VIEWER_BINARY_NAME} working directory for debugging."
            )
    endif (NOT UNATTENDED)

    add_custom_command(
        TARGET ${VIEWER_BINARY_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS
          -E
          copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/messages/message_template.msg
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/app_settings/message_template.msg
        COMMENT "Copying message_template.msg to the runtime folder."
        )

    add_custom_command(
        TARGET ${VIEWER_BINARY_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS
          -E
          copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/message.xml
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/app_settings/message.xml
        COMMENT "Copying message.xml to the runtime folder."
        )

    add_dependencies(${VIEWER_BINARY_NAME} copy_win_libs)

    if (EXISTS ${CMAKE_SOURCE_DIR}/copy_win_scripts)
      add_dependencies(${VIEWER_BINARY_NAME} copy_win_scripts)
    endif (EXISTS ${CMAKE_SOURCE_DIR}/copy_win_scripts)

      add_custom_command(
        OUTPUT ${CMAKE_CFG_INTDIR}/touched.bat
        COMMAND ${PYTHON_EXECUTABLE}
        ARGS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
          --artwork=${ARTWORK_DIR}
          --branding_id=${VIEWER_BRANDING_ID}
          --build=${CMAKE_CURRENT_BINARY_DIR}
          --channel=${VIEWER_CHANNEL}
          --configuration=${CMAKE_CFG_INTDIR}
          --dest=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
          --grid=${GRID}
          --login_channel=${VIEWER_LOGIN_CHANNEL}
          --source=${CMAKE_CURRENT_SOURCE_DIR}
          --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/touched.bat
		  ${MANIFEST_LIBRARIES}
        DEPENDS
            ${VIEWER_BINARY_NAME}
            ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        )

      add_custom_command(
        OUTPUT ${CMAKE_CFG_INTDIR}/copy_touched.bat
        COMMAND ${PYTHON_EXECUTABLE}
        ARGS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
          --artwork=${ARTWORK_DIR}
          --actions=copy
          --branding_id=${VIEWER_BRANDING_ID}
          --build=${CMAKE_CURRENT_BINARY_DIR}
          --channel=${VIEWER_CHANNEL}
          --configuration=${CMAKE_CFG_INTDIR}
          --dest=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
          --grid=${GRID}
          --login_channel=${VIEWER_LOGIN_CHANNEL}
          --source=${CMAKE_CURRENT_SOURCE_DIR}
          --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/copy_touched.bat
		  ${MANIFEST_LIBRARIES}
        DEPENDS
            ${VIEWER_BINARY_NAME}
            ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        COMMENT "Performing viewer_manifest copy"
        )

    add_dependencies(${VIEWER_BINARY_NAME} SLPlugin media_plugin_quicktime media_plugin_webkit basic_plugin_filepicker winmm_shim)

    if (PACKAGE)
      add_custom_target(package ALL DEPENDS ${CMAKE_CFG_INTDIR}/touched.bat)
    endif (PACKAGE)
endif (WINDOWS)

# *NOTE - this list is very sensitive to ordering, test carefully on all
# platforms if you change the releative order of the entries here.
# In particular, cmake 2.6.4 (when buidling with linux/makefile generators)
# appears to sometimes de-duplicate redundantly listed dependencies improperly.
# To work around this, higher level modules should be listed before the modules
# that they depend upon. -brad
target_link_libraries(${VIEWER_BINARY_NAME}
    ${GOOGLE_PERFTOOLS_LIBRARIES}
    ${LLAUDIO_LIBRARIES}
    ${LLAUDIO_VORBIS_LIBRARIES}
    ${LLCHARACTER_LIBRARIES}
    ${LLIMAGE_LIBRARIES}
    ${LLIMAGEJ2COJ_LIBRARIES}
    ${LLINVENTORY_LIBRARIES}
    ${LLMESSAGE_LIBRARIES}
    ${LLPLUGIN_LIBRARIES}
    ${STATEMACHINE_LIBRARIES}
    ${LLPRIMITIVE_LIBRARIES}
    ${LLRENDER_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${LLUI_LIBRARIES}
    ${LLVFS_LIBRARIES}
    ${LLWINDOW_LIBRARIES}
    ${LLXML_LIBRARIES}
#    ${LSCRIPT_LIBRARIES}
    ${LLMATH_LIBRARIES}
    ${LLCOMMON_LIBRARIES}
    ${NDOF_LIBRARY}
    ${viewer_LIBRARIES}
    ${Boost_CONTEXT_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${DBUSGLIB_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${FMODWRAPPER_LIBRARY} # must come after LLAudio
    ${GLOD_LIBRARIES}
    ${APRUTIL_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${SDL_LIBRARY}
    ${SMARTHEAP_LIBRARY}
    ${UI_LIBRARIES}
    ${WINDOWS_LIBRARIES}
    ${XMLRPCEPI_LIBRARIES}
    ${ELFIO_LIBRARIES}
    ${HUNSPELL_LIBRARY}
    ${LLPHYSICSEXTENSIONS_LIBRARIES}
    ${LLAPPEARANCE_LIBRARIES}
    )

if (LINUX)
  add_custom_command(
      OUTPUT secondlife-stripped
      COMMAND strip
      ARGS --strip-debug -o secondlife-stripped ${VIEWER_BINARY_NAME}
      DEPENDS ${VIEWER_BINARY_NAME}
      )

  set(product ${VIEWER_BRANDING_NAME_CAMELCASE}-${ARCH}-${viewer_VERSION})

  if (FMODEX)
    if(MANIFEST_LIBRARIES)
      set(MANIFEST_LIBRARIES "${MANIFEST_LIBRARIES}|${FMODEX_LIBRARY}")
    else(MANIFEST_LIBRARIES)
      set(MANIFEST_LIBRARIES "--extra_libraries=${FMODEX_LIBRARY}")
    endif(MANIFEST_LIBRARIES)
  endif (FMODEX)

  add_custom_command(
      OUTPUT ${product}.tar.bz2
      COMMAND ${PYTHON_EXECUTABLE}
      ARGS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        --arch=${ARCH}
        --artwork=${ARTWORK_DIR}
        --branding_id=${VIEWER_BRANDING_ID}
        --build=${CMAKE_CURRENT_BINARY_DIR}
        --buildtype=${CMAKE_BUILD_TYPE}
        --channel=${VIEWER_CHANNEL}
        --dest=${CMAKE_CURRENT_BINARY_DIR}/packaged
        --grid=${GRID}
        --installer_name=${product}
        --login_channel=${VIEWER_LOGIN_CHANNEL}
        --source=${CMAKE_CURRENT_SOURCE_DIR}
        --standalone=${STANDALONE}
        --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.touched
		  ${MANIFEST_LIBRARIES}
      DEPENDS
        secondlife-stripped
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      )

  add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.copy_touched
      COMMAND ${PYTHON_EXECUTABLE}
      ARGS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        --arch=${ARCH}
        --actions=copy
        --artwork=${ARTWORK_DIR}
        --branding_id=${VIEWER_BRANDING_ID}
        --build=${CMAKE_CURRENT_BINARY_DIR}
        --buildtype=${CMAKE_BUILD_TYPE}
        --channel=${VIEWER_CHANNEL}
        --dest=${CMAKE_CURRENT_BINARY_DIR}/packaged
        --grid=${GRID}
        --installer_name=${product}
        --login_channel=${VIEWER_LOGIN_CHANNEL}
        --source=${CMAKE_CURRENT_SOURCE_DIR}
        --standalone=${STANDALONE}
		  ${MANIFEST_LIBRARIES}
      DEPENDS
        secondlife-stripped
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      )

  add_dependencies(${VIEWER_BINARY_NAME} SLPlugin media_plugin_gstreamer010 media_plugin_webkit basic_plugin_filepicker)
  add_custom_target(copy_l_viewer_manifest ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.copy_touched)

  if (PACKAGE)
    add_custom_target(package ALL DEPENDS ${product}.tar.bz2)
    add_dependencies(package copy_l_viewer_manifest)
  endif (PACKAGE)
endif (LINUX)

if (DARWIN)
  set(product ${VIEWER_BRANDING_NAME})
  set_target_properties(
    ${VIEWER_BINARY_NAME}
    PROPERTIES
    OUTPUT_NAME "${product}"
    MACOSX_BUNDLE_INFO_STRING "A stable third-party Second Life viewer."
    MACOSX_BUNDLE_ICON_FILE "${VIEWER_BRANDING_ID}.icns"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${VIEWER_BRANDING_NAME}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${viewer_VERSION}"
    MACOSX_BUNDLE_BUNDLE_NAME "${VIEWER_BRANDING_NAME}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${viewer_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${viewer_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright 2010 Balseraph Software Group"
    )

  add_custom_command(
    TARGET ${VIEWER_BINARY_NAME} POST_BUILD
    COMMAND ${PYTHON_EXECUTABLE}
    ARGS
      ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      --actions=copy
      --artwork=${ARTWORK_DIR}
      --branding_id=${VIEWER_BRANDING_ID}
      --build=${CMAKE_CURRENT_BINARY_DIR}
      --buildtype=${CMAKE_BUILD_TYPE}
      --configuration=${CMAKE_CFG_INTDIR}
      --dest=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${product}.app
      --grid=${GRID}
      --source=${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${VIEWER_BINARY_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
    )

  add_dependencies(${VIEWER_BINARY_NAME} SLPlugin media_plugin_quicktime media_plugin_webkit basic_plugin_filepicker)

  if (FMODEX)
    add_custom_command(OUTPUT "${FMODEX_LIBRARY}"
                      COMMAND cp "${FMODEX_ORIG_LIBRARY}" "${FMODEX_LIBRARY}"
                      COMMAND install_name_tool -id "@executable_path/../Resources/libfmodex.dylib" ${FMODEX_LIBRARY}
                      DEPENDS "${FMODEX_ORIG_LIBRARY}")
    add_custom_target(fmodex_modified_library DEPENDS "${FMODEX_LIBRARY}")
    add_dependencies(${VIEWER_BINARY_NAME} fmodex_modified_library)
  endif (FMODEX)

  if (PACKAGE)
      add_custom_target(package ALL DEPENDS ${VIEWER_BINARY_NAME})

      add_custom_command(
        TARGET package POST_BUILD
        COMMAND ${PYTHON_EXECUTABLE}
        ARGS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
          --artwork=${ARTWORK_DIR}
          --branding_id=${VIEWER_BRANDING_ID}
          --build=${CMAKE_CURRENT_BINARY_DIR}
          --buildtype=${CMAKE_BUILD_TYPE}
          --channel=${VIEWER_CHANNEL}
          --configuration=${CMAKE_CFG_INTDIR}
          --dest=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${product}.app
          --grid=${GRID}
          --login_channel=${VIEWER_LOGIN_CHANNEL}
          --source=${CMAKE_CURRENT_SOURCE_DIR}
          --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.touched
        DEPENDS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      )
# Viewer-Development doesn't have this duplicate of the above command, here..
# this may be a botched merge, but Mac seems to be picky, so I'm leaving it ~Liru
      add_custom_command(
        TARGET package POST_BUILD
        COMMAND ${PYTHON_EXECUTABLE}
        ARGS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
          --artwork=${ARTWORK_DIR}
          --branding_id=${VIEWER_BRANDING_ID}
          --build=${CMAKE_CURRENT_BINARY_DIR}
          --buildtype=${CMAKE_BUILD_TYPE}
          --channel=${VIEWER_CHANNEL}
          --configuration=${CMAKE_CFG_INTDIR}
          --dest=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${product}.app
          --grid=${GRID}
          --login_channel=${VIEWER_LOGIN_CHANNEL}
          --source=${CMAKE_CURRENT_SOURCE_DIR}
          --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.touched
        DEPENDS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      )

  endif (PACKAGE)
endif (DARWIN)

if (INSTALL)
  include(${CMAKE_CURRENT_SOURCE_DIR}/ViewerInstall.cmake)
endif (INSTALL)

if (PACKAGE)

  set(SYMBOL_SEARCH_DIRS "")
  if (WINDOWS)
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
    set(VIEWER_SYMBOL_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/secondlife-symbols-windows.tar.bz2")
    # slplugin.exe failing symbols dump - need to debug, might have to do with updated version of google breakpad
    # set(VIEWER_EXE_GLOBS "${VIEWER_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX} slplugin.exe")
    set(VIEWER_EXE_GLOBS "${VIEWER_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    set(VIEWER_LIB_GLOB "*${CMAKE_SHARED_MODULE_SUFFIX}")
    set(VIEWER_COPY_MANIFEST copy_w_viewer_manifest)
  endif (WINDOWS)
  if (DARWIN)
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
    # *TODO: Generate these search dirs in the cmake files related to each binary.
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/llplugin/slplugin/${CMAKE_CFG_INTDIR}")
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/media_plugins/gstreamer010/${CMAKE_CFG_INTDIR}")
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/media_plugins/quicktime/${CMAKE_CFG_INTDIR}")
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/media_plugins/webkit/${CMAKE_CFG_INTDIR}")
    set(VIEWER_SYMBOL_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/secondlife-symbols-darwin.tar.bz2")
    set(VIEWER_EXE_GLOBS "'Singularity' SLPlugin")
    set(VIEWER_LIB_GLOB "*.dylib")
  endif (DARWIN)
  if (LINUX)
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_CURRENT_BINARY_DIR}/packaged")
    set(VIEWER_SYMBOL_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/secondlife-symbols-linux.tar.bz2")
    set(VIEWER_EXE_GLOBS "singularity-do-not-run-directly SLPlugin")
    set(VIEWER_LIB_GLOB "*${CMAKE_SHARED_MODULE_SUFFIX}*")
    set(VIEWER_COPY_MANIFEST copy_l_viewer_manifest)
  endif (LINUX)

  if(RELEASE_CRASH_REPORTING OR NON_RELEASE_CRASH_REPORTING)
  if(CMAKE_CFG_INTDIR STREQUAL ".")
      set(LLBUILD_CONFIG ${CMAKE_BUILD_TYPE})
  else(CMAKE_CFG_INTDIR STREQUAL ".")
      # set LLBUILD_CONFIG to be a shell variable evaluated at build time
      # reflecting the configuration we are currently building.
      set(LLBUILD_CONFIG ${CMAKE_CFG_INTDIR})
  endif(CMAKE_CFG_INTDIR STREQUAL ".")
  add_custom_command(OUTPUT "${VIEWER_SYMBOL_FILE}"
    COMMAND "${PYTHON_EXECUTABLE}"
    ARGS
      "${CMAKE_CURRENT_SOURCE_DIR}/generate_breakpad_symbols.py"
      "${LLBUILD_CONFIG}"
      "${SYMBOL_SEARCH_DIRS}"
      "${VIEWER_EXE_GLOBS}"
      "${VIEWER_LIB_GLOB}"
      "${LIBS_PREBUILT_DIR}/${LL_ARCH_DIR}/bin/dump_syms"
      "${VIEWER_SYMBOL_FILE}"
    DEPENDS generate_breakpad_symbols.py
        VERBATIM)

  add_custom_target(generate_breakpad_symbols DEPENDS "${VIEWER_SYMBOL_FILE}")
  add_dependencies(generate_breakpad_symbols "${VIEWER_BINARY_NAME}" "${VIEWER_COPY_MANIFEST}")
  add_dependencies(package generate_breakpad_symbols)
  endif(RELEASE_CRASH_REPORTING OR NON_RELEASE_CRASH_REPORTING)
endif (PACKAGE)

# Add tests
if (LL_TESTS)
	ADD_VIEWER_BUILD_TEST(llagentaccess viewer)
	#ADD_VIEWER_BUILD_TEST(llworldmap viewer)
	#ADD_VIEWER_BUILD_TEST(llworldmipmap viewer)
	ADD_VIEWER_BUILD_TEST(lltextureinfo viewer)
	ADD_VIEWER_BUILD_TEST(lltextureinfodetails viewer)
	ADD_VIEWER_BUILD_TEST(lltexturestatsuploader viewer)
	#ADD_VIEWER_COMM_BUILD_TEST(lltranslate viewer "")
endif (LL_TESTS)

# Don't do these for DARWIN or LINUX here -- they're taken care of by viewer_manifest.py
if (WINDOWS)

  get_target_property(BUILT_LLCOMMON llcommon LOCATION)

    set_target_properties(llcommon
        PROPERTIES
        LINK_FLAGS "/debug /NODEFAULTLIB:LIBCMT"
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:\"LIBCMT;LIBCMTD;MSVCRT\""
        )

  add_custom_command(
      TARGET ${VIEWER_BINARY_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS
      -E
      copy_if_different
      ${BUILT_LLCOMMON}
      ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
      COMMENT "Copying llcommon.dll to the runtime folder."
      )

  get_target_property(BUILT_SLPLUGIN SLPlugin LOCATION)
  add_custom_command(
      TARGET ${VIEWER_BINARY_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS
        -E
        copy_if_different
        ${BUILT_SLPLUGIN}
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
      COMMENT "Copying SLPlugin executable to the runtime folder."
      )

  get_target_property(BUILT_WEBKIT_PLUGIN media_plugin_webkit LOCATION)
    add_custom_command(
        TARGET ${VIEWER_BINARY_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS
          -E
          copy_if_different
          ${BUILT_WEBKIT_PLUGIN}
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/llplugin
        COMMENT "Copying WebKit Plugin to the runtime folder."
        )

  get_target_property(BUILT_QUICKTIME_PLUGIN media_plugin_quicktime LOCATION)
    add_custom_command(
        TARGET ${VIEWER_BINARY_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS
          -E
          copy_if_different
          ${BUILT_QUICKTIME_PLUGIN}
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/llplugin
        COMMENT "Copying Quicktime Plugin to the runtime folder."
        )

	get_target_property(BUILT_FILEPICKER_PLUGIN basic_plugin_filepicker LOCATION)
    add_custom_command(
        TARGET ${VIEWER_BINARY_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS
          -E
          copy_if_different
          ${BUILT_FILEPICKER_PLUGIN}
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/llplugin
        COMMENT "Copying filepicker Plugin to the runtime folder."
        )

	get_target_property(BUILT_WINMM_SHIM_PLUGIN winmm_shim LOCATION)
    add_custom_command(
        TARGET ${VIEWER_BINARY_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS
          -E
          copy_if_different
          ${BUILT_WINMM_SHIM_PLUGIN}
          ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
        COMMENT "Copying winmm.dll to the runtime folder."
        )
  # Copying the mime_types.xml file to app_settings
  set(mime_types_source "${CMAKE_SOURCE_DIR}/newview/skins/default/xui/en-us")
  set(mime_types_dest "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/app_settings")
  add_custom_command(
      TARGET ${VIEWER_BINARY_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS 
	 -E
	 copy_if_different
	 ${mime_types_source}/mime_types_windows.xml 
	 ${mime_types_dest}/mime_types.xml
      COMMENT "Copying mime_types_windows.xml to mime_types.xml."
      )
  
endif (WINDOWS)

if (DARWIN)
# Don't do this here -- it's taken care of by viewer_manifest.py
#  add_custom_command(TARGET ${VIEWER_BINARY_NAME} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/../libraries/universal-darwin/lib/release/libllqtwebkit.dylib ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/llplugin/
#    DEPENDS ${CMAKE_SOURCE_DIR}/../libraries/universal-darwin/lib/release/libllqtwebkit.dylib
#  )
endif (DARWIN)
