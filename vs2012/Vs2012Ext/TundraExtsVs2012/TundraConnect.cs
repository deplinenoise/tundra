using System;
using Extensibility;
using EnvDTE;
using EnvDTE80;
using Microsoft.VisualStudio.CommandBars;
using Microsoft.VisualStudio.VCProjectEngine;
using System.Resources;
using System.Reflection;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;

namespace TundraExtsVs2012
{
	/// <summary>The object for implementing an Add-in.</summary>
	/// <seealso class='IDTExtensibility2' />
	public class TundraConnect : IDTExtensibility2, IDTCommandTarget
	{
		/// <summary>Implements the constructor for the Add-in object. Place your initialization code within this method.</summary>
		public TundraConnect()
		{
		}

		/// <summary>Implements the OnConnection method of the IDTExtensibility2 interface. Receives notification that the Add-in is being loaded.</summary>
		/// <param term='application'>Root object of the host application.</param>
		/// <param term='connectMode'>Describes how the Add-in is being loaded.</param>
		/// <param term='addInInst'>Object representing this Add-in.</param>
		/// <seealso class='IDTExtensibility2' />
		public void OnConnection(object application, ext_ConnectMode connectMode, object addInInst, ref Array custom)
		{
			m_ApplicationObject = (DTE2)application;
			m_AddInInstance = (AddIn)addInInst;
      m_OutputWindow = (OutputWindow) m_ApplicationObject.Windows.Item(EnvDTE.Constants.vsWindowKindOutput).Object;
      m_TundraPane = m_OutputWindow.OutputWindowPanes.Add("Tundra");

      if (connectMode == ext_ConnectMode.ext_cm_UISetup)
      {
        object[] contextGUIDS = new object[] { };
        Commands2 commands = (Commands2)m_ApplicationObject.Commands;
        string toolsMenuName = "Tools";

        //Place the command on the tools menu.
        //Find the MenuBar command bar, which is the top-level command bar holding all the main menu items:
        Microsoft.VisualStudio.CommandBars.CommandBar menuBarCommandBar = ((Microsoft.VisualStudio.CommandBars.CommandBars)m_ApplicationObject.CommandBars)["MenuBar"];

        //Find the Tools command bar on the MenuBar command bar:
        CommandBarControl toolsControl = menuBarCommandBar.Controls[toolsMenuName];
        CommandBarPopup toolsPopup = (CommandBarPopup)toolsControl;


        //This try/catch block can be duplicated if you wish to add multiple commands to be handled by your Add-in,
        //  just make sure you also update the QueryStatus/Exec method to include the new command names.
        try
        {
          //Add a command to the Commands collection:
          Command command = commands.AddNamedCommand2(m_AddInInstance, "BuildCurrentFile", "Build Current File with Tundra", "Builds the current source file using the Tundra build system",
                        true, 483, ref contextGUIDS,
                        (int)vsCommandStatus.vsCommandStatusSupported + (int)vsCommandStatus.vsCommandStatusEnabled,
                        (int)vsCommandStyle.vsCommandStylePictAndText, vsCommandControlType.vsCommandControlTypeButton);

          //Add a control for the command to the tools menu:
          if ((command != null) && (toolsPopup != null))
          {
            command.AddControl(toolsPopup.CommandBar, 1);
          }
        }
        catch (System.ArgumentException ex)
        {
          m_TundraPane.OutputString(ex.Message);
          m_TundraPane.OutputString("\n");
          //If we are here, then the exception is probably because a command with that name
          //  already exists. If so there is no need to recreate the command and we can 
          //  safely ignore the exception.
        }
      }
      else
      {
        m_TundraPane.OutputString("UI setup already done.\n");
      }
		}

		/// <summary>Implements the OnDisconnection method of the IDTExtensibility2 interface. Receives notification that the Add-in is being unloaded.</summary>
		/// <param term='disconnectMode'>Describes how the Add-in is being unloaded.</param>
		/// <param term='custom'>Array of parameters that are host application specific.</param>
		/// <seealso class='IDTExtensibility2' />
		public void OnDisconnection(ext_DisconnectMode disconnectMode, ref Array custom)
		{
		}

		/// <summary>Implements the OnAddInsUpdate method of the IDTExtensibility2 interface. Receives notification when the collection of Add-ins has changed.</summary>
		/// <param term='custom'>Array of parameters that are host application specific.</param>
		/// <seealso class='IDTExtensibility2' />		
		public void OnAddInsUpdate(ref Array custom)
		{
		}

		/// <summary>Implements the OnStartupComplete method of the IDTExtensibility2 interface. Receives notification that the host application has completed loading.</summary>
		/// <param term='custom'>Array of parameters that are host application specific.</param>
		/// <seealso class='IDTExtensibility2' />
		public void OnStartupComplete(ref Array custom)
		{
		}

		/// <summary>Implements the OnBeginShutdown method of the IDTExtensibility2 interface. Receives notification that the host application is being unloaded.</summary>
		/// <param term='custom'>Array of parameters that are host application specific.</param>
		/// <seealso class='IDTExtensibility2' />
		public void OnBeginShutdown(ref Array custom)
		{
		}
		
		/// <summary>Implements the QueryStatus method of the IDTCommandTarget interface. This is called when the command's availability is updated</summary>
		/// <param term='commandName'>The name of the command to determine state for.</param>
		/// <param term='neededText'>Text that is needed for the command.</param>
		/// <param term='status'>The state of the command in the user interface.</param>
		/// <param term='commandText'>Text requested by the neededText parameter.</param>
		/// <seealso class='Exec' />
		public void QueryStatus(string commandName, vsCommandStatusTextWanted neededText, ref vsCommandStatus status, ref object commandText)
		{
			if(neededText == vsCommandStatusTextWanted.vsCommandStatusTextWantedNone)
			{
				if(commandName == "TundraExtsVs2012.TundraConnect.BuildCurrentFile")
				{
          status = (vsCommandStatus)vsCommandStatus.vsCommandStatusSupported | vsCommandStatus.vsCommandStatusEnabled;
					return;
				}
			}
		}

    struct ContextInfo
    {
      public Document Document;
      public VCProject Project;
      public VCNMakeTool NMakeTool;
    };

    // Figure out the prereqs needed to run tundra on a source file.
    private bool GetContext(out ContextInfo info)
    {
      info.Document = null;
      info.Project = null;
      info.NMakeTool = null;

      var doc = m_ApplicationObject.ActiveDocument;

      if (doc == null)
        return false;

      info.Document = doc;

      var proj_item = doc.ProjectItem;
      if (proj_item == null)
        return false;

      var project = proj_item.ContainingProject;
      if (project == null)
        return false;

      var vc_proj = project.Object as VCProject;
      if (null == vc_proj || vc_proj.keyword != "MakeFileProj")
        return false;

      info.Project = vc_proj;

      var cfg = vc_proj.Configurations as IVCCollection;
      if (null == cfg)
        return false;

      var vc_cfg = cfg.Item(project.ConfigurationManager.ActiveConfiguration.ConfigurationName) as VCConfiguration;
      if (null == vc_cfg)
        return false;

      var tools = vc_cfg.Tools as IVCCollection;
      if (null == tools)
        return false;

      VCNMakeTool nmake_tool = tools.Item("NMake Tool");
      if (null == nmake_tool)
        return false;

      info.NMakeTool = nmake_tool;
      return true;
    }

		/// <summary>Implements the Exec method of the IDTCommandTarget interface. This is called when the command is invoked.</summary>
		/// <param term='commandName'>The name of the command to execute.</param>
		/// <param term='executeOption'>Describes how the command should be run.</param>
		/// <param term='varIn'>Parameters passed from the caller to the command handler.</param>
		/// <param term='varOut'>Parameters passed from the command handler to the caller.</param>
		/// <param term='handled'>Informs the caller if the command was handled or not.</param>
		/// <seealso class='Exec' />
		public void Exec(string commandName, vsCommandExecOption executeOption, ref object varIn, ref object varOut, ref bool handled)
		{
			handled = false;
			if(executeOption == vsCommandExecOption.vsCommandExecOptionDoDefault)
			{
				if(commandName == "TundraExtsVs2012.TundraConnect.BuildCurrentFile")
				{
          ContextInfo context;
          if (GetContext(out context))
          {
            string cmdline = context.NMakeTool.BuildCommandLine;
            Match m = m_TundraBuildRegex.Match(cmdline);
            if (m.Success)
            {
              if (!context.Document.Saved)
                context.Document.Save();

              var tundra_path = m.Groups[1].ToString();
              var dir = m.Groups[2].ToString();
              var config = m.Groups[3].ToString();
              var full_file_name = context.Document.FullName;

              try
              {
                using (System.Diagnostics.Process p = new System.Diagnostics.Process())
                {
                  p.StartInfo.UseShellExecute = false;
                  p.StartInfo.CreateNoWindow = true;
                  p.StartInfo.RedirectStandardOutput = true;
                  p.StartInfo.RedirectStandardError = true;
                  p.StartInfo.WorkingDirectory = context.Project.ProjectDirectory;
                  p.StartInfo.FileName = tundra_path;
                  p.StartInfo.Arguments = "-C " + dir + " \"" + full_file_name + "\" " + config;

                  p.OutputDataReceived += OnOutputDataReceived;
                  p.ErrorDataReceived += OnOutputDataReceived;

                  m_TundraPane.Activate();
                  m_TundraPane.Clear();
                  m_OutputWindow.Parent.Activate();

                  m_TundraPane.OutputString(tundra_path);
                  m_TundraPane.OutputString(" ");
                  m_TundraPane.OutputString(p.StartInfo.Arguments);
                  m_TundraPane.OutputString("\n");

                  p.Start();
                  p.BeginOutputReadLine();
                  p.BeginErrorReadLine();
                  p.WaitForExit();

                  m_TundraPane.OutputString(String.Format("Exit code: {0}\n", p.ExitCode));
                }
              }
              catch (Exception ex)
              {
                m_TundraPane.OutputString("Failed to launch Tundra\n");
                m_TundraPane.OutputString(ex.Message + "\n");
              }
            }
          }
          else
          {
            m_ApplicationObject.ExecuteCommand("Build.Compile");
          }

					handled = true;
					return;
				}
			}
		}

    void OnOutputDataReceived(object sender, DataReceivedEventArgs e)
    {
      m_TundraPane.OutputString(e.Data + "\n");
    }

    private Regex m_TundraBuildRegex = new Regex("^(\"?.*?tundra2\\.exe\"?)\\s+-C\\s+(\"?[^\"]+\"?).*?([\\w_]+-[\\w_]+-[\\w_]+-[\\w_]+).*$");
		private DTE2 m_ApplicationObject;
		private AddIn m_AddInInstance;
    private OutputWindow m_OutputWindow;
    private OutputWindowPane m_TundraPane;
	}
}