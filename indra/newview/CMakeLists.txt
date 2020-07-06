# -*- cmake -*-

project(viewer)

include(00-Common)
# DON'T move Linking.cmake to its place in the alphabetized list below: it
# sets variables on which the 3p .cmake files depend.
include(Linking)

include(Boost)
include(BuildPackagesInfo)
include(BuildVersion)
include(BuildBranding)
include(CMakeCopyIfDifferent)
include(CrashPad)
include(DBusGlib)
include(FMODSTUDIO)
include(GeneratePrecompiledHeader)
include(GLOD)
include(Hunspell)
include(LLAddBuildTest)
include(LLAppearance)
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
include(NDOF)
include(NVAPI)
include(OPENAL)
include(OpenGL)
include(OpenSSL)
include(StateMachine)
include(TemplateCheck)
include(UI)
include(ViewerMiscLibs)
include(WinManifest)
include(ZLIB)
include(URIPARSER)


if(USE_FMODSTUDIO)
  include_directories(${FMODSTUDIO_INCLUDE_DIR})
endif(USE_FMODSTUDIO)

if(USE_CRASHPAD)
  include_directories(${CRASHPAD_INCLUDE_DIRS})
endif(USE_CRASHPAD)

include_directories(
    ${STATEMACHINE_INCLUDE_DIRS}
    ${DBUSGLIB_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${GLOD_INCLUDE_DIR}
    ${LLAUDIO_INCLUDE_DIRS}
    ${LLCHARACTER_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLPHYSICSEXTENSIONS_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLINVENTORY_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLPRIMITIVE_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLUI_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${HUNSPELL_INCLUDE_DIR}
    ${OPENAL_LIB_INCLUDE_DIRS}
    ${LLAPPEARANCE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

set(viewer_SOURCE_FILES
    NACLantispam.cpp
    aihttpview.cpp
    aixmllindengenepool.cpp
    alfloaterregiontracker.cpp
    aoremotectrl.cpp
    aosystem.cpp
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
    generichandlers.cpp
    groupchatlistener.cpp
    hbfloatergrouptitles.cpp
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
    llaisapi.cpp
    llagent.cpp
    llagentaccess.cpp
    llagentbenefits.cpp
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
    llautoreplace.cpp
    llavataractions.cpp
    llavatarpropertiesprocessor.cpp
    llavatarrenderinfoaccountant.cpp
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
    llcontrolavatar.cpp
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
    lldrawpoolmaterials.cpp
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
    llexperiencelog.cpp
    llexternaleditor.cpp
    llface.cpp
    llfasttimerview.cpp
    llfavoritesbar.cpp
    llfeaturemanager.cpp
    llfirstuse.cpp
    llflexibleobject.cpp
    llfloaterabout.cpp
    llfloateractivespeakers.cpp
    llfloaterauction.cpp
    llfloaterautoreplacesettings.cpp
    llfloateravatar.cpp
    llfloateravatarinfo.cpp
    llfloateravatarlist.cpp
    llfloateravatarpicker.cpp
    llfloateravatartextures.cpp
    llfloaterbanduration.cpp
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
    llfloaterdestinations.cpp
    llfloaterdirectory.cpp
    llfloaterdisplayname.cpp
    llfloatereditui.cpp
    llfloaterenvsettings.cpp
    llfloaterevent.cpp
    llfloaterexperiencepicker.cpp
    llfloaterexperienceprofile.cpp
    llfloaterexperiences.cpp
    llfloaterexploreanimations.cpp
    llfloaterexploresounds.cpp
    llfloaterfeed.cpp
    llfloaterfonttest.cpp
    llfloaterfriends.cpp
    llfloatergesture.cpp
    llfloatergodtools.cpp
    llfloatergroupbulkban.cpp
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
    llfloaterland.cpp
    llfloaterlandholdings.cpp
    llfloaterlandmark.cpp
    llfloatermap.cpp
    llfloatermarketplacelistings.cpp
    llfloatermediafilter.cpp
    llfloatermediasettings.cpp
    llfloatermemleak.cpp
    llfloatermessagelog.cpp
    llfloatermodelpreview.cpp
    llfloatermodeluploadbase.cpp
    llfloatermute.cpp
    llfloaternamedesc.cpp
    llfloaternotificationsconsole.cpp
    llfloaterobjectiminfo.cpp
    llfloaterobjectweights.cpp
    llfloateropenobject.cpp
    llfloaterparcel.cpp
    llfloaterpathfindingcharacters.cpp
    llfloaterpathfindinglinksets.cpp
    llfloaterpathfindingobjects.cpp
    llfloaterperms.cpp
    llfloaterpostcard.cpp
    llfloaterpostprocess.cpp
    llfloaterpreference.cpp
    llfloaterproperties.cpp
    llfloaterregiondebugconsole.cpp
    llfloaterregioninfo.cpp
    llfloaterregionrestarting.cpp
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
    llhttpretrypolicy.cpp
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
    llimprocessing.cpp
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
    lllandmarkactions.cpp
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
    llmediafilter.cpp
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
    llnameui.cpp
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
    llpanelexperiencelisteditor.cpp
    llpanelexperiencelog.cpp
    llpanelexperiencepicker.cpp
    llpanelexperiences.cpp
    llpanelface.cpp
    llpanelgeneral.cpp
    llpanelgroup.cpp
    llpanelgroupbulk.cpp
    llpanelgroupbulkban.cpp
    llpanelgroupexperiences.cpp
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
    llsculptidsize.cpp
    llselectmgr.cpp
    llshareavatarhandler.cpp
    llskinningutil.cpp
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
    lluiavatar.cpp
    lluploaddialog.cpp
    lluploadfloaterobservers.cpp
    llurl.cpp
    llurldispatcher.cpp
    llurlhistory.cpp
    llurlwhitelist.cpp
    lluserauth.cpp
    llvectorperfoptions.cpp
    llversioninfo.cpp
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
    llvieweroctree.cpp
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
    pipeline.cpp
    qtoolalign.cpp
    rlvactions.cpp
    rlvcommon.cpp
    rlvextensions.cpp
    rlvfloaters.cpp
    rlvhandler.cpp
    rlvhelper.cpp
    rlvinventory.cpp
    rlvlocks.cpp
    rlvui.cpp
    scriptcounter.cpp
    sgmemstat.cpp
    shcommandhandler.cpp
    shupdatechecker.cpp
    shfloatermediaticker.cpp
    wlfPanel_AdvSettings.cpp
    )

set(viewer_HEADER_FILES
    CMakeLists.txt
    ViewerInstall.cmake

    NACLantispam.h
    aihttpview.h
    aixmllindengenepool.h
    alfloaterregiontracker.h
    aoremotectrl.h
    aostate.h
    aosystem.h
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
    llaisapi.h
    llagent.h
    llagentaccess.h
    llagentbenefits.h
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
    llautoreplace.h
    llavataractions.h
    llavatarpropertiesprocessor.h
    llavatarrenderinfoaccountant.h
    llbox.h
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
    llcontrolavatar.h
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
    lldrawpoolmaterials.h
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
    llexperiencelog.h
    llexternaleditor.h
    llface.h
    llfasttimerview.h
    llfavoritesbar.h
    llfeaturemanager.h
    llfirstuse.h
    llflexibleobject.h
    llfloaterabout.h
    llfloateractivespeakers.h
    llfloaterauction.h
    llfloaterautoreplacesettings.h
    llfloateravatar.h
    llfloateravatarinfo.h
    llfloateravatarlist.h
    llfloateravatarpicker.h
    llfloateravatartextures.h
    llfloaterbanduration.h
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
    llfloaterdestinations.h
    llfloaterdirectory.h
    llfloaterdisplayname.h
    llfloatereditui.h
    llfloaterenvsettings.h
    llfloaterevent.h
    llfloaterexperiencepicker.h
    llfloaterexperienceprofile.h
    llfloaterexperiences.h
    llfloaterexploreanimations.h
    llfloaterexploresounds.h
    llfloaterfeed.h
    llfloaterfonttest.h
    llfloaterfriends.h
    llfloatergesture.h
    llfloatergodtools.h
    llfloatergroupbulkban.h
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
    llfloaterland.h
    llfloaterlandholdings.h
    llfloaterlandmark.h
    llfloatermap.h
    llfloatermarketplacelistings.h
    llfloatermediafilter.h
    llfloatermediasettings.h
    llfloatermemleak.h
    llfloatermessagelog.h
    llfloatermodelpreview.h
    llfloatermodeluploadbase.h
    llfloatermute.h
    llfloaternamedesc.h
    llfloaternotificationsconsole.h
    llfloaterobjectiminfo.h
    llfloaterobjectweights.h
    llfloateropenobject.h
    llfloaterparcel.h
    llfloaterpathfindingcharacters.h
    llfloaterpathfindinglinksets.h
    llfloaterpathfindingobjects.h
    llfloaterperms.h
    llfloaterpostcard.h
    llfloaterpostprocess.h
    llfloaterpreference.h
    llfloaterproperties.h
    llfloaterregiondebugconsole.h
    llfloaterregioninfo.h
    llfloaterregionrestarting.h
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
    llhttpretrypolicy.h
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
    llimprocessing.h
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
    lllandmarkactions.h
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
    llmediafilter.h
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
    llnameui.h
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
    llpanelexperiencelisteditor.h
    llpanelexperiencelog.h
    llpanelexperiencepicker.h
    llpanelexperiences.h
    llpanelface.h
    llpanelgeneral.h
    llpanelgroup.h
    llpanelgroupbulk.h
    llpanelgroupbulkban.h
    llpanelgroupbulkimpl.h
    llpanelgroupexperiences.h
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
    llsculptidsize.h
    llselectmgr.h
    llsimplestat.h
    llskinningutil.h
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
    lluiavatar.h
    lluploaddialog.h
    lluploadfloaterobservers.h
    llurl.h
    llurldispatcher.h
    llurlhistory.h
    llurlwhitelist.h
    lluserauth.h
    llvectorperfoptions.h
    llvelocitybar.h
    llversioninfo.h
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
    llvieweroctree.h
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
    pipeline.h
    qtoolalign.h
    rlvactions.h
    rlvcommon.h
    rlvdefines.h
    rlvextensions.h
    rlvfloaters.h
    rlvhandler.h
    rlvhelper.h
    rlvinventory.h
    rlvlocks.h
    rlvui.h
    roles_constants.h
    scriptcounter.h
    sgmemstat.h
    shcommandhandler.h
    shfloatermediaticker.h
    shupdatechecker.h
    wlfPanel_AdvSettings.h
    )

source_group("CMake Rules" FILES ViewerInstall.cmake)

#build_data.json creation moved to viewer_manifest.py MAINT-6413
# the viewer_version.txt file created here is for passing to viewer_manifest and autobuild
# the summary.json file is created for the benefit of the TeamCity builds, where
#   it is used to provide descriptive information to the build results page
add_custom_target(generate_viewer_version ALL
                  COMMAND ${CMAKE_COMMAND} -E echo ${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION} > ${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
                  COMMENT "Generating viewer_version.txt for manifest processing"
                  )

set_source_files_properties(
   llimpanel.cpp
   llversioninfo.cpp
   PROPERTIES
   DEPENDS generate_viewer_version  # dummy dependency to force recompile every time
   COMPILE_DEFINITIONS "${VIEWER_CHANNEL_VERSION_DEFINES}" # see BuildVersion.cmake
   )

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
    macview.r
    gpu_table.txt
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

  if (USE_NVAPI)
    set(APPVWRW32_COMPILE_FLAGS "${APPVWRW32_COMPILE_FLAGS} -DUSE_NVAPI=1")
  else (USE_NVAPI)
    set(APPVWRW32_COMPILE_FLAGS "${APPVWRW32_COMPILE_FLAGS} -UUSE_NVAPI")
  endif (USE_NVAPI)
  set_source_files_properties(llappviewerwin32.cpp PROPERTIES COMPILE_FLAGS "${APPVWRW32_COMPILE_FLAGS}")

    # Replace the icons with the appropriate ones for the channel
    # ('test' is the default)
    set(ICON_PATH "default")
    set(VIEWER_MACOSX_PHASE "d")
    message("Copying icons for ${ICON_PATH}")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/icons/${ICON_PATH}/viewer.ico"
        "${CMAKE_CURRENT_SOURCE_DIR}/res/viewer_icon.ico"
      )
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/icons/${ICON_PATH}/viewer_256.BMP"
        "${CMAKE_CURRENT_SOURCE_DIR}/res/viewer_icon.BMP"
      )
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/icons/${ICON_PATH}/viewer_256.BMP"
        "${CMAKE_CURRENT_SOURCE_DIR}/res-sdl/viewer_icon.BMP"
      )

    # Add resource files to the project.
    # viewerRes.rc is the only buildable file, but
    # the rest are all dependencies of it.
    set(viewer_RESOURCE_FILES
        res/arrow.cur
        res/arrowcop.cur
        res/arrowcopmulti.cur
        res/arrowdrag.cur
        res/circleandline.cur
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
        res-sdl/viewer_icon.BMP
        res/viewer_icon.BMP
        res/viewer_icon.ico
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


    list(APPEND viewer_RESOURCE_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/viewerRes.rc
        )

    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/viewerRes.rc
      PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/res"
      )

    source_group("Resource Files" FILES ${viewer_RESOURCE_FILES})

    list(APPEND viewer_SOURCE_FILES ${viewer_RESOURCE_FILES})

# see EXP-1765 - theory is opengl32.lib needs to be included before gdi32.lib (windows libs)
    set(viewer_LIBRARIES
        opengl32
        advapi32
        comdlg32
        dinput8
        dxguid
        gdi32
        kernel32
        odbc32
        odbccp32
        ole32
        oleaut32
        shell32
        user32
        Vfw32
        wer
        Wbemuuid
        winspool
        Normaliz
        )
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
    app_settings/keys.xml
    app_settings/keysZQSD.xml
    app_settings/keywords.ini
    app_settings/logcontrol.xml
    app_settings/low_graphics.xml
    app_settings/mid_graphics.xml
    app_settings/settings.xml
    app_settings/settings_ascent.xml
    app_settings/settings_ascent_coa.xml
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
    packages-info.txt
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
  list(APPEND LLSTARTUP_COMPILE_DEFINITIONS "LL_OPENAL=1")
endif (OPENAL)

if (USE_FMODSTUDIO)
  list(APPEND LLSTARTUP_COMPILE_DEFINITIONS "LL_FMODSTUDIO=1")
  set_source_files_properties(llpanellogin.cpp PROPERTIES COMPILE_DEFINITIONS "LL_FMODSTUDIO=1")
endif (USE_FMODSTUDIO)

set_source_files_properties(llstartup.cpp PROPERTIES COMPILE_DEFINITIONS "${LLSTARTUP_COMPILE_DEFINITIONS}")

if (LIBVLCPLUGIN)
    set_source_files_properties(llfloaterabout.cpp PROPERTIES COMPILE_DEFINITIONS "VLCPLUGIN=1")
else (LIBVLCPLUGIN)
    set_source_files_properties(llfloaterabout.cpp PROPERTIES COMPILE_DEFINITIONS "VLCPLUGIN=0")
endif (LIBVLCPLUGIN)

list(APPEND viewer_SOURCE_FILES ${viewer_HEADER_FILES})

set_source_files_properties(${viewer_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

add_executable(${VIEWER_BINARY_NAME}
    WIN32
    MACOSX_BUNDLE
    ${viewer_SOURCE_FILES}
    )

if(USE_PRECOMPILED_HEADERS)
  target_precompiled_header(${VIEWER_BINARY_NAME}
      ${CMAKE_CURRENT_SOURCE_DIR}/llviewerprecompiledheaders.h
      ${CMAKE_CURRENT_SOURCE_DIR}/llviewerprecompiledheaders.cpp)
endif(USE_PRECOMPILED_HEADERS)

option(PACKAGE "Add a package target that builds an installer package." ON)

if (WINDOWS)
    set(release_flags "/MAP")

    set_target_properties(${VIEWER_BINARY_NAME}
        PROPERTIES
        LINK_FLAGS "/debug /NODEFAULTLIB:LIBCMT /SUBSYSTEM:WINDOWS ${GOOGLE_PERFTOOLS_LINKER_FLAGS}"
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:\"LIBCMT;LIBCMTD;MSVCRT\" ${EXTRA_LINKER_FLAGS_DEBUG}"
        LINK_FLAGS_RELEASE "${release_flags} ${EXTRA_LINKER_FLAGS_RELEASE}"
		LINK_FLAGS_RELWITHDEBINFO "${release_flags} ${EXTRA_LINKER_FLAGS_RELEASE}"
        )

    if(GEN_IS_MULTI_CONFIG)
      set(VIEWER_BUILD_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
      set_target_properties(${VIEWER_BINARY_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
      set(VIEWER_BUILD_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
      set_target_properties(${VIEWER_BINARY_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${VIEWER_BUILD_DEST_DIR}")
    endif()

    add_dependencies(${VIEWER_BINARY_NAME} generate_viewer_version)

    # If adding a file to viewer_manifest.py in the WindowsManifest.construct() method, be sure to add the dependency
    # here.
    # *NOTE:Mani - This is a crappy hack to have important dependencies for the viewer_manifest copy action
    # be met. I'm looking forward to a source-code split-up project next year that will address this kind of thing.
    # In the meantime, if you have any ideas on how to easily maintain one list, either here or in viewer_manifest.py
    # and have the build deps get tracked *please* tell me about it.

    if(NOT DISABLE_TCMALLOC)
      # Configure a var for tcmalloc location, if used.
      # Note the need to specify multiple names explicitly.
      set(GOOGLE_PERF_TOOLS_SOURCE
        ${SHARED_LIB_STAGING_DIR}/Release/libtcmalloc_minimal.dll
        ${SHARED_LIB_STAGING_DIR}/RelWithDebInfo/libtcmalloc_minimal.dll
        )
    endif(NOT DISABLE_TCMALLOC)

    set(COPY_INPUT_DEPENDENCIES
      # The following commented dependencies are determined at variably at build time. Can't do this here.
      #${SHARED_LIB_STAGING_DIR}/${CMAKE_CFG_INTDIR}/libtcmalloc_minimal.dll => None ... Skipping libtcmalloc_minimal.dll
      ${CMAKE_SOURCE_DIR}/../etc/message.xml
      ${CMAKE_SOURCE_DIR}/../scripts/messages/message_template.msg
      ${SHARED_LIB_STAGING_DIR}/Release/glod.dll
      ${SHARED_LIB_STAGING_DIR}/RelWithDebInfo/glod.dll
      ${SHARED_LIB_STAGING_DIR}/Release/SLVoice.exe
      ${SHARED_LIB_STAGING_DIR}/Release/vivoxplatform.dll
      ${GOOGLE_PERF_TOOLS_SOURCE}
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses-win32.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/featuretable.txt
      SLPlugin
      media_plugin_libvlc
      media_plugin_cef
	  basic_plugin_filepicker
      )

    if (ADDRESS_SIZE EQUAL 64)
        list(APPEND COPY_INPUT_DEPENDENCIES
            ${SHARED_LIB_STAGING_DIR}/Release/vivoxsdk_x64.dll
            ${SHARED_LIB_STAGING_DIR}/Release/ortp_x64.dll
            )
    else (ADDRESS_SIZE EQUAL 64)
        list(APPEND COPY_INPUT_DEPENDENCIES
            ${SHARED_LIB_STAGING_DIR}/Release/vivoxsdk.dll
            ${SHARED_LIB_STAGING_DIR}/Release/ortp.dll
            )
    endif (ADDRESS_SIZE EQUAL 64)

    if (USE_FMODSTUDIO)
      if (ADDRESS_SIZE EQUAL 64)
        list(APPEND COPY_INPUT_DEPENDENCIES
             ${SHARED_LIB_STAGING_DIR}/Release/fmod64.dll
             ${SHARED_LIB_STAGING_DIR}/RelWithDebInfo/fmod64.dll
             ${SHARED_LIB_STAGING_DIR}/Debug/fmodL64.dll
            )
      else (ADDRESS_SIZE EQUAL 64)
        list(APPEND COPY_INPUT_DEPENDENCIES
             ${SHARED_LIB_STAGING_DIR}/Release/fmod.dll
             ${SHARED_LIB_STAGING_DIR}/RelWithDebInfo/fmod.dll
            )
      endif (ADDRESS_SIZE EQUAL 64)
    endif (USE_FMODSTUDIO)

    if(MSVC_IDE)
      set(VIEWER_BUILD_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
    else()
      set(VIEWER_BUILD_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/packaged")
    endif()

    add_custom_command(
      OUTPUT  ${VIEWER_BUILD_DEST_DIR}/copy_touched.bat
      COMMAND ${PYTHON_EXECUTABLE}
      ARGS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        --actions=copy
        --arch=${ARCH}
        --artwork=${ARTWORK_DIR}
        --branding_id=${VIEWER_BRANDING_ID}
        --build=${CMAKE_CURRENT_BINARY_DIR}
        --buildtype=${CMAKE_BUILD_TYPE}
        --configuration=${VIEWER_BUILD_DEST_DIR}
        --dest=${VIEWER_BUILD_DEST_DIR}
        --grid=${GRID}
        --channel=${VIEWER_CHANNEL}
        --versionfile=${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
        --source=${CMAKE_CURRENT_SOURCE_DIR}
        --touch=${VIEWER_BUILD_DEST_DIR}/copy_touched.bat
      DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        generate_viewer_version
        stage_third_party_libs
		llcommon
        ${COPY_INPUT_DEPENDENCIES}
      COMMENT "Performing viewer_manifest copy"
      )

    add_custom_target(copy_w_viewer_manifest ALL DEPENDS ${VIEWER_BUILD_DEST_DIR}/copy_touched.bat)

    add_dependencies(${VIEWER_BINARY_NAME} stage_third_party_libs llcommon copy_w_viewer_manifest)

    if (EXISTS ${CMAKE_SOURCE_DIR}/copy_win_scripts)
      add_dependencies(${VIEWER_BINARY_NAME} copy_win_scripts)
    endif (EXISTS ${CMAKE_SOURCE_DIR}/copy_win_scripts)

    add_dependencies(${VIEWER_BINARY_NAME}
      SLPlugin
      )

    EMBED_MANIFEST(${VIEWER_BINARY_NAME} 1)

    if (PACKAGE)

      add_custom_command(
        OUTPUT ${VIEWER_BUILD_DEST_DIR}/touched.bat
        COMMAND ${PYTHON_EXECUTABLE}
        ARGS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
          --arch=${ARCH}
          --artwork=${ARTWORK_DIR}
          --branding_id=${VIEWER_BRANDING_ID}
          --build=${CMAKE_CURRENT_BINARY_DIR}
          --buildtype=${CMAKE_BUILD_TYPE}
          --channel=${VIEWER_CHANNEL}
          --versionfile=${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
          --configuration=${VIEWER_BUILD_DEST_DIR}
          --dest=${VIEWER_BUILD_DEST_DIR}
          --grid=${GRID}
          --source=${CMAKE_CURRENT_SOURCE_DIR}
	  --touch=${VIEWER_BUILD_DEST_DIR}/touched.bat
        DEPENDS
            ${VIEWER_BINARY_NAME}
            ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
            ${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
            ${COPY_INPUT_DEPENDENCIES}
        )

      add_custom_target(llpackage ALL DEPENDS
        ${VIEWER_BUILD_DEST_DIR}/touched.bat
        windows-setup-build-all
        )
    endif (PACKAGE)
endif (WINDOWS)

# *NOTE: - this list is very sensitive to ordering, test carefully on all
# platforms if you change the releative order of the entries here.
# In particular, cmake 2.6.4 (when buidling with linux/makefile generators)
# appears to sometimes de-duplicate redundantly listed dependencies improperly.
# To work around this, higher level modules should be listed before the modules
# that they depend upon. -brad
target_link_libraries(${VIEWER_BINARY_NAME}
    ${PNG_PRELOAD_ARCHIVES}
    ${ZLIB_PRELOAD_ARCHIVES}
    ${GOOGLE_PERFTOOLS_LIBRARIES}
    ${CRASHPAD_LIBRARIES}
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
    ${LLMATH_LIBRARIES}
    ${LLCOMMON_LIBRARIES}
    ${NDOF_LIBRARY}
    ${NVAPI_LIBRARY}
    ${URIPARSER_LIBRARY}
    ${viewer_LIBRARIES}
    ${Boost_CONTEXT_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${DBUSGLIB_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${FMOD_LIBRARY} # must come after LLAudio
    ${GLOD_LIBRARIES}
    ${APRUTIL_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SDL_LIBRARY}
    ${SMARTHEAP_LIBRARY}
    ${UI_LIBRARIES}
    ${WINDOWS_LIBRARIES}
    ${XMLRPCEPI_LIBRARIES}
    ${CRYPTO_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CRYPTO_LIBRARIES}
    ${HUNSPELL_LIBRARY}
    ${LLPHYSICSEXTENSIONS_LIBRARIES}
    ${LLAPPEARANCE_LIBRARIES}
    absl::flat_hash_map
    absl::node_hash_map
    nlohmann_json::nlohmann_json
    fmt::fmt
    )

if (LINUX)
  set(product ${VIEWER_CHANNEL_NOSPACE}-${ARCH}-${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION})

  # These are the generated targets that are copied to package/
  set(COPY_INPUT_DEPENDENCIES
    ${VIEWER_BINARY_NAME}
    SLPlugin
    media_plugin_gstreamer010
    llcommon
    )

  add_dependencies(${VIEWER_BINARY_NAME} generate_viewer_version)

  add_custom_command(
      OUTPUT ${product}.tar.xz
      COMMAND ${PYTHON_EXECUTABLE}
      ARGS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        --arch=${ARCH}
        --artwork=${ARTWORK_DIR}
        --branding_id=${VIEWER_BRANDING_ID}
        --build=${CMAKE_CURRENT_BINARY_DIR}
        --buildtype=${CMAKE_BUILD_TYPE}
        --channel=${VIEWER_CHANNEL}
        --versionfile=${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
        --configuration=${CMAKE_CFG_INTDIR}
        --dest=${CMAKE_CURRENT_BINARY_DIR}/packaged
        --grid=${GRID}
        --source=${CMAKE_CURRENT_SOURCE_DIR}
        --standalone=${STANDALONE}
        --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.touched
      DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
        ${COPY_INPUT_DEPENDENCIES}
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
        --configuration=${CMAKE_CFG_INTDIR}
        --dest=${CMAKE_CURRENT_BINARY_DIR}/packaged
        --grid=${GRID}
        --channel=${VIEWER_CHANNEL}
        --versionfile=${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
        --source=${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      ${COPY_INPUT_DEPENDENCIES}
    COMMENT "Performing viewer_manifest copy"
      )

  add_dependencies(${VIEWER_BINARY_NAME} SLPlugin media_plugin_gstreamer010 basic_plugin_filepicker)

  if (PACKAGE)
    add_custom_target(llpackage ALL DEPENDS ${product}.tar.xz)
    check_message_template(llpackage)
  else (PACKAGE)
    add_custom_target(copy_l_viewer_manifest ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.copy_touched)
  endif (PACKAGE)
endif (LINUX)

if (DARWIN)
  set(product ${VIEWER_BRANDING_NAME})
  set(MACOSX_BUNDLE_INFO_STRING "A stable third-party Second Life viewer.")
  set(MACOSX_BUNDLE_ICON_FILE "viewer.icns")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "${VIEWER_BRANDING_NAME}")
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "${VIEWER_CHANNEL} ${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION}")
  set(MACOSX_BUNDLE_BUNDLE_NAME "${VIEWER_BRANDING_NAME}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${VIEWER_SHORT_VERSION}")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${${ROOT_PROJECT_NAME}_VERSION}")
  set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2010-2016 Siana Gearz")

  set_target_properties(
    ${VIEWER_BINARY_NAME}
    PROPERTIES
    OUTPUT_NAME "${product}"
    MACOSX_BUNDLE_INFO_PLIST
    "${CMAKE_CURRENT_SOURCE_DIR}/Info-SecondLife.plist"
    )

  configure_file(
     "${CMAKE_CURRENT_SOURCE_DIR}/Info-SecondLife.plist"
     "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${product}.app/Contents/Info.plist"
    )

  add_dependencies(${VIEWER_BINARY_NAME} generate_viewer_version)

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
      --extra_libraries="${MANIFEST_LIBRARIES}"
    DEPENDS
      ${VIEWER_BINARY_NAME}
      ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
    )

  add_dependencies(${VIEWER_BINARY_NAME} SLPlugin media_plugin_libvlc media_plugin_cef basic_plugin_filepicker)

  if (PACKAGE)
      add_custom_target(llpackage ALL DEPENDS ${VIEWER_BINARY_NAME})
      add_dependencies(llpackage generate_viewer_version)

      add_custom_command(
        TARGET llpackage POST_BUILD
        COMMAND ${PYTHON_EXECUTABLE}
        ARGS
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
          --arch=${ARCH}
          --artwork=${ARTWORK_DIR}
          --build=${CMAKE_CURRENT_BINARY_DIR}
          --buildtype=${CMAKE_BUILD_TYPE}
          --channel=${VIEWER_CHANNEL}
          --configuration=${CMAKE_CFG_INTDIR}
          --dest=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${product}.app
          --grid=${GRID}
          --channel=${VIEWER_CHANNEL}
          --versionfile=${CMAKE_CURRENT_BINARY_DIR}/viewer_version.txt
          --source=${CMAKE_CURRENT_SOURCE_DIR}
          --touch=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/.${product}.touched
        DEPENDS
		  ${VIEWER_BINARY_NAME}
          ${CMAKE_CURRENT_SOURCE_DIR}/viewer_manifest.py
      )
  endif (PACKAGE)
endif (DARWIN)

if (INSTALL)
  include(${CMAKE_CURRENT_SOURCE_DIR}/ViewerInstall.cmake)
endif (INSTALL)

if (FALSE)
  # Breakpad symbol-file generation
  set(SYMBOL_NAME ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${VIEWER_CHANNEL_NOSPACE}-${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION}-symbols)

  set(SYMBOL_SEARCH_DIRS "")
  if (WINDOWS)
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
    set(VIEWER_SYMBOL_FILE "${SYMBOL_NAME}-${AUTOBUILD_PLATFORM_NAME}.tar.bz2")
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
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/plugins/gstreamer010/${CMAKE_CFG_INTDIR}")
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/plugins/quicktime/${CMAKE_CFG_INTDIR}")
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_BINARY_DIR}/plugins/webkit/${CMAKE_CFG_INTDIR}")
    set(VIEWER_SYMBOL_FILE "${SYMBOL_NAME}-darwin.tar.bz2")
    set(VIEWER_EXE_GLOBS "'${VIEWER_BRANDING_NAME}' SLPlugin")
    set(VIEWER_LIB_GLOB "*.dylib")
  endif (DARWIN)
  if (LINUX)
    list(APPEND SYMBOL_SEARCH_DIRS "${CMAKE_CURRENT_BINARY_DIR}/packaged")
    set(VIEWER_SYMBOL_FILE "${SYMBOL_NAME}-${AUTOBUILD_PLATFORM_NAME}.tar.bz2")
    set(VIEWER_EXE_GLOBS "do-not-directly-run-${VIEWER_BINARY_NAME} SLPlugin")
    set(VIEWER_LIB_GLOB "*${CMAKE_SHARED_MODULE_SUFFIX}*")
    set(VIEWER_COPY_MANIFEST copy_l_viewer_manifest)
  endif (LINUX)

  if(GEN_IS_MULTI_CONFIG)
      set(LLBUILD_CONFIG ${CMAKE_BUILD_TYPE})
  else()
      # set LLBUILD_CONFIG to be a shell variable evaluated at build time
      # reflecting the configuration we are currently building.
      set(LLBUILD_CONFIG ${CMAKE_CFG_INTDIR})
  endif()
  add_custom_command(OUTPUT "${VIEWER_SYMBOL_FILE}"
    COMMAND "${PYTHON_EXECUTABLE}"
    ARGS
      "${CMAKE_CURRENT_SOURCE_DIR}/generate_breakpad_symbols.py"
      "${LLBUILD_CONFIG}"
      "${SYMBOL_SEARCH_DIRS}"
      "${VIEWER_EXE_GLOBS}"
      "${VIEWER_LIB_GLOB}"
      "${AUTOBUILD_INSTALL_DIR}/bin/dump_syms"
      "${VIEWER_SYMBOL_FILE}"
    DEPENDS generate_breakpad_symbols.py
        VERBATIM)

    add_custom_target(generate_symbols DEPENDS "${VIEWER_SYMBOL_FILE}" ${VIEWER_BINARY_NAME} "${VIEWER_COPY_MANIFEST}")
    add_dependencies(generate_symbols ${VIEWER_BINARY_NAME})
    if (WINDOWS OR LINUX)
      add_dependencies(generate_symbols "${VIEWER_COPY_MANIFEST}")
    endif (WINDOWS OR LINUX)

  add_dependencies(llpackage generate_symbols)
endif ()

# Add tests
if (LL_TESTS)
endif (LL_TESTS)

check_message_template(${VIEWER_BINARY_NAME})

