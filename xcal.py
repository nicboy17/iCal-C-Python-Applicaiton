#***************************************************************************
# xcal.py -- iCalendar gui for utility functions. Uses calutil.c and caltool.c
#         -- includes CalDav and MySQL Database (A4)
#              -- python3
#
# Created: Mar.01 2016
# Modified: Mar.21 2016 - for A4
# Author: Nick Major #0879292
# Contact: nmajor@mail.uoguelph.ca
#***************************************************************************
#!/usr/bin/env python3.5

from tkinter import *
from tkinter.tix import *
from tkinter import messagebox
from tkinter.constants import *
from tkinter.scrolledtext import ScrolledText
from tkinter.filedialog import askopenfilename
from tkinter.filedialog import asksaveasfilename
from subprocess import *
import os
import Cal
import requests
import sys
import mysql.connector
from mysql.connector import errorcode
import getpass


class xcal(Frame):

    # Initialize window
    def __init__(self, parent):
        Frame.__init__(self, parent)
        self.parent = parent

        self.init = False
        self.todoInit = False
        self.name = None
        self.server = False
        self.combineName = ""
        self.url = ""
        self.username = ""
        self.passw = ""
        self.unSaved = False
        self.displayName = ""
        self.count = 0
        self.result = []   # create empty list to hold resultmajors
        self.undoList = [] # undo todo remove from last save
        self.compList = []
        self.comp = []
        self.filterDate = False
        self.filterFile = False

        self.initUI()
        self.parent.protocol("WM_DELETE_WINDOW", self.onExit)

    # create main window
    def initUI(self):
        self.parent.title("xcal")
        self.parent.resizable(width=True, height=True)

        # Main objects
        global fileMenu
        global todoMenu
        global helpMenu
        global daveMenu
        global dataBase
        global view
        global log
        global showSelected
        global extractElements
        global extractProps

        def exitShort(event):
            self.onExit()

        def todoShort(event):
            if(self.result != []):
                self.Todo()

        def openCalFile(event):
            self.callback()

        def undoShort(event):
            if(self.todoInit is True):
                self.Undo()

        def saveShort(event):
            if(self.unSaved is True):
                self.saveFile()


        self.bind_all("<Control-x>", exitShort)
        self.bind_all("<Control-t>", todoShort)
        self.bind_all("<Control-o>", openCalFile)
        self.bind_all("<Control-z>", undoShort)
        self.bind_all("<Control-s>", saveShort)

        # MenuBar
        menubar = Menu(self.parent)
        self.parent.config(menu=menubar)
        fileMenu = Menu(menubar)
        fileMenu.add_command(label="Open", command=self.callback,
                             accelerator="Ctrl+O")
        fileMenu.add_command(label="Save", command=self.saveFile,
                             accelerator="Ctrl+S", state=DISABLED)
        fileMenu.add_command(label="Save as", command=self.saveFileAs, state=DISABLED)
        fileMenu.add_command(label="Combine", command=self.Combine,
                             state=DISABLED)
        fileMenu.add_command(label="Filter", command=self.getFilter,
                             state=DISABLED)
        fileMenu.add_command(label="Exit", command=self.onExit,
                             accelerator="Ctrl+X")
        menubar.add_cascade(label="File", menu=fileMenu)

        todoMenu = Menu(menubar)
        todoMenu.add_command(label="To-do List", command=self.Todo,
                             accelerator="Ctrl+T", state=DISABLED)
        todoMenu.add_command(label="Undo", command=self.Undo,
                             accelerator="Ctrl+Z", state=DISABLED)
        menubar.add_cascade(label="Todo", menu=todoMenu)

        helpMenu = Menu(menubar)
        helpMenu.add_command(label="Date Mask", command=self.getDateMsk)
        helpMenu.add_command(label="About xcal", command=self.About)
        menubar.add_cascade(label="Help", menu=helpMenu)

        daveMenu = Menu(menubar)
        daveMenu.add_command(label="Account", command=self.accountPrompt)
        daveMenu.add_command(label="Export", command=self.Export, state=DISABLED)
        daveMenu.add_command(label="Import", command=self.Import, state=DISABLED)
        menubar.add_cascade(label="CalDAV", menu=daveMenu)

        # NEW
        dataBase = Menu(menubar)
        dataBase.add_command(label="Store All", command=self.dataStoreAll, state=DISABLED)
        dataBase.add_command(label="Store Selected", command=self.dataStoreSelected, state=DISABLED)
        dataBase.add_command(label="Clear", command=self.dataClear, state=DISABLED)
        dataBase.add_command(label="Status", command=self.dataStatus, state=DISABLED)
        dataBase.add_command(label="Query", command=self.dataQuery, state=DISABLED)
        menubar.add_cascade(label="Database", menu=dataBase)

        # NEW
        if(len(sys.argv) > 1):
            for x in range(0,3):
                try:
                    if(len(sys.argv) == 2):
                        self.db = mysql.connector.connect(user=sys.argv[1], host="dursley.socs.uoguelph.ca", password=getpass.getpass(), database=sys.argv[1])
                    elif(len(sys.argv) == 3):
                        self.db = mysql.connector.connect(user=sys.argv[1], host="dursley.socs.uoguelph.ca", password=getpass.getpass(), database=sys.argv[2])
                    self.cursor = self.db.cursor()

                    query = "CREATE TABLE IF NOT EXISTS ORGANIZER( org_id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR( 60 ) NOT NULL, contact VARCHAR( 60 ) NOT NULL );"
                    self.cursor.execute(query)
                    query = "CREATE TABLE IF NOT EXISTS EVENT( event_id INT AUTO_INCREMENT PRIMARY KEY, summary VARCHAR( 60 ) NOT NULL, start_time DATETIME NOT NULL, location VARCHAR( 60 ), organizer INT, FOREIGN KEY(organizer) REFERENCES ORGANIZER(org_id) ON DELETE CASCADE );"
                    self.cursor.execute(query)
                    query = "CREATE TABLE IF NOT EXISTS TODO( todo_id INT AUTO_INCREMENT PRIMARY KEY, summary VARCHAR( 60 ) NOT NULL, priority SMALLINT, organizer INT, FOREIGN KEY(organizer) REFERENCES ORGANIZER(org_id) ON DELETE CASCADE );"
                    self.cursor.execute(query)

                    self.db.commit()
                    dataBase.entryconfig("Query", state="normal")
                    dataBase.entryconfig("Status", state="normal")
                    self.checkStatus()

                    break
                except mysql.connector.Error as err:
                    print("failed")
                    if(x == 2):
                        quit()

        # fileView Frame
        fileView = Frame(self.parent)
        fileView.pack()
        fileView.place(x=10, y=25)

        scrollbar = Scrollbar(fileView)
        scrollbar.pack(side=RIGHT, fill=Y)

        text = Label(text="No.    Name               Props     Subs      Summary")
        text.place(x=10, y=4)

        def clearSelect(self):
            current = int(view.curselection()[0])
            view.select_clear(current)
            showSelected.config(state='disabled')
            dataBase.entryconfig("Store Selected", state = "disabled")

        def clearSelected(self):
            showSelected.config(state='normal')
            dataBase.entryconfig("Store Selected", state = "normal")

        view = Listbox(fileView, width=68, height=20, yscrollcommand=scrollbar.set,
                       selectmode=SINGLE, font=("Courier", 8, "normal"),
                       selectbackground="grey")
        view.bind('<Double-Button-1>', clearSelect)
        view.bind('<<ListboxSelect>>', clearSelected)

        view.pack()
        scrollbar.config(command=view.yview)

        # LogView Frame
        logView = Frame(self.parent, bd=2)
        logView.pack()
        logView.place(x=10, y=345)

        log = ScrolledText(logView, width=88, height=14, bg="grey")
        log.pack(side=LEFT)

        logLabel = Label(text="Log:")
        logLabel.place(x=15, y=325)

        # Buttons
        clear=Button(self.parent, text="Clear", command=self.clearLog)
        clear.place(x=535, y=317)

        showSelected = Button(self.parent, text="Show Selected",
                              command=self.selected, state=DISABLED)
        showSelected.place(x=515, y=25)

        extractElements = Button(self.parent, text="Extract Events",
                                 command=self.extractEvents, state=DISABLED)
        extractElements.place(x=515, y=65)

        extractProps = Button(self.parent, text="Extract X- Props",
                              command=self.extractProps, state=DISABLED)
        extractProps.place(x=515, y=105)

        # Get Datemsk variable for getDate_r
        if(os.environ.get('DATEMSK') is None and self.init is False):
            global dateMsk
            self.init = True
            dateMsk = Toplevel()
            dateMsk.attributes("-topmost", True)
            dateMsk.title("DATEMSK")
            dateMsk.geometry('350x125+%d+%d' % (dateMsk.winfo_screenwidth()/2-175,
                             dateMsk.winfo_screenheight()/2-65))
            dateMsk.resizable(width=FALSE, height=FALSE)
            label = Label(dateMsk, text="Would you like to set the DATEMSK variable now?")
            label.pack(side="top", fill="x", pady=5)

            B2 = Button(dateMsk, text="Yes", command=self.getDateMsk)
            B2.place(x=210, y=75)

            B1 = Button(dateMsk, text="Not Now", command=self.destroyDatemsk)
            B1.place(x=75, y=75)

            def close(event):
                dateMsk.destroy()

            dateMsk.bind('<Escape>', close)
            dateMsk.transient(self.parent)
            dateMsk.grab_set()
            self.parent.wait_window(dateMsk)

    # NEW
    def dataClear(self):
        query = "TRUNCATE TABLE EVENT;"
        self.cursor.execute(query)

        query = "TRUNCATE TABLE TODO;"
        self.cursor.execute(query)

        query = "TRUNCATE TABLE ORGANIZER;"
        self.cursor.execute(query)

        self.db.commit()
        self.dataStatus()
        self.checkStatus()


    # NEW
    def dataStatus(self):
        query = "SELECT COUNT(*) FROM ORGANIZER"
        self.cursor.execute(query)
        org = (self.cursor.fetchone()[0])

        query = "SELECT COUNT(*) FROM EVENT"
        self.cursor.execute(query)
        evt = (self.cursor.fetchone()[0])

        query = "SELECT COUNT(*) FROM TODO"
        self.cursor.execute(query)
        todo = (self.cursor.fetchone()[0])
        log.insert(INSERT, "Database has %d organizers, %d events, %d to-do items."%(org, evt, todo) )
        log.see(END)
        log.insert(INSERT, "\n\n")

    def checkStatus(self):
        query = "SELECT COUNT(*) FROM ORGANIZER"
        self.cursor.execute(query)
        org = (self.cursor.fetchone()[0])

        query = "SELECT COUNT(*) FROM EVENT"
        self.cursor.execute(query)
        evt = (self.cursor.fetchone()[0])

        query = "SELECT COUNT(*) FROM TODO"
        self.cursor.execute(query)
        todo = (self.cursor.fetchone()[0])

        if(org > 0 or evt > 0 or todo > 0):
            dataBase.entryconfig("Clear", state = "normal")
        else:
            dataBase.entryconfig("Clear", state = "disabled")

    # NEW
    def dataStoreAll(self):
        for x in range(0, len(self.storeSQL)):
            if( x in self.compList):
                query = "SELECT * FROM ORGANIZER WHERE name = '%s'"%(str(self.storeSQL[x][1]))
                self.cursor.execute(query)
                result = self.cursor.fetchall()
                if( result == [] ):
                    query = """INSERT INTO ORGANIZER (name, contact) VALUES ('%s','%s');"""%(self.storeSQL[x][1], self.storeSQL[x][2])
                    self.cursor.execute(query)
                    self.db.commit()

                query = "SELECT org_id FROM ORGANIZER WHERE name = '%s'"%(str(self.storeSQL[x][1]))
                self.cursor.execute(query)
                result = self.cursor.fetchall()
                organId = result[0][0]
                if(str(self.storeSQL[x][0]) == "VEVENT"):
                    query = "SELECT * FROM EVENT WHERE summary = '%s' and start_time = '%s'"%(str(self.storeSQL[x][3]), self.storeSQL[x][4])
                    self.cursor.execute(query)
                    result = self.cursor.fetchall()
                    if( result == [] ):
                        add_event = "INSERT INTO EVENT (summary, start_time, location, organizer) VALUES (%s, %s, %s, %s)"
                        data_event = (self.storeSQL[x][3], self.storeSQL[x][4], self.storeSQL[x][5], organId)
                        self.cursor.execute(add_event, data_event)
                        self.db.commit()

                elif(str(self.storeSQL[x][0]) == "VTODO"):
                    query = "SELECT * FROM TODO WHERE summary = '%s'"%(str(self.storeSQL[x][3]))
                    self.cursor.execute(query)
                    result = self.cursor.fetchall()
                    if( result == [] ):
                        add_todo = "INSERT INTO TODO (summary, priority, organizer) VALUES (%s, %s, %s)"
                        data_todo = (self.storeSQL[x][3], self.storeSQL[x][6], organId)
                        self.cursor.execute(add_todo, data_todo)
                        self.db.commit()
        self.dataStatus()
        self.checkStatus()

    # NEW
    def dataStoreSelected(self):
        sel = int(view.curselection()[0])

        query = "SELECT * FROM ORGANIZER WHERE name = '%s'"%(str(self.storeSQL[sel][1]))
        self.cursor.execute(query)
        result = self.cursor.fetchall()
        if( result == [] ):
            query = """INSERT INTO ORGANIZER (name, contact) VALUES ('%s','%s');"""%(self.storeSQL[sel][1], self.storeSQL[sel][2])
            self.cursor.execute(query)
            self.db.commit()

        query = "SELECT org_id FROM ORGANIZER WHERE name = '%s'"%(str(self.storeSQL[sel][1]))
        self.cursor.execute(query)
        result = self.cursor.fetchall()
        organId = result[0][0]
        if(str(self.storeSQL[sel][0]) == "VEVENT"):
            query = "SELECT * FROM EVENT WHERE summary = '%s' and start_time = '%s'"%(str(self.storeSQL[sel][3]), self.storeSQL[sel][4])
            self.cursor.execute(query)
            result = self.cursor.fetchall()
            if( result == [] ):
                add_event = "INSERT INTO EVENT (summary, start_time, location, organizer) VALUES (%s, %s, %s, %s)"
                data_event = (self.storeSQL[sel][3], self.storeSQL[sel][4], self.storeSQL[sel][5], organId)
                self.cursor.execute(add_event, data_event)
                self.db.commit()

        elif(str(self.storeSQL[sel][0]) == "VTODO"):
            query = "SELECT * FROM TODO WHERE summary = '%s'"%(str(self.storeSQL[sel][3]))
            self.cursor.execute(query)
            result = self.cursor.fetchall()
            if( result == [] ):
                add_todo = "INSERT INTO TODO (summary, priority, organizer) VALUES (%s, %s, %s)"
                data_todo = (self.storeSQL[sel][3], self.storeSQL[sel][6], organId)
                self.cursor.execute(add_todo, data_todo)
                self.db.commit()
        self.dataStatus()
        self.checkStatus()

    # NEW
    def dataQuery(self):
        global queryW
        queryW = Tk()
        queryW.lift()
        queryW.attributes("-topmost", True)
        queryW.wm_title("Query")
        queryW.geometry('450x500+%d+%d' % (queryW.winfo_screenwidth()/2 - 225, queryW.winfo_screenheight()/2 - 250))
        queryW.resizable(width=FALSE, height=FALSE)
        dataBase.entryconfig("Query", state="disabled")
        queryW.protocol("WM_DELETE_WINDOW", self.qExit)

        text = Text(queryW, width=57, height=3)
        text.place(x=40, y=240)
        text.insert(INSERT, "SELECT")
        text.config(state="disabled")

        text2 = ScrolledText(queryW, width=59, height=8, bg="grey")
        text2.place(x=10, y=340)

        def selTodo():
            clearBoxes()
            self.adHoc = "summary"
            text.config(state="disabled")
            T1.config(state="disabled")
            T2.config(state="disabled")
            T3.config(state="disabled")
            T4.config(state="disabled")
            T5.config(state="normal")

        def organizerEN():
            clearBoxes()
            self.adHoc = "organizer"
            T1.config(state="normal")
            text.config(state="disabled")
            T2.config(state="disabled")
            T3.config(state="disabled")
            T4.config(state="disabled")
            T5.config(state="disabled")

        def locationEN():
            clearBoxes()
            self.adHoc = "location"
            T2.config(state="normal")
            text.config(state="disabled")
            T1.config(state="disabled")
            T3.config(state="disabled")
            T4.config(state="disabled")
            T5.config(state="disabled")

        def timeEN():
            clearBoxes()
            self.adHoc = "time"
            T3.config(state="normal")
            text.config(state="disabled")
            T1.config(state="disabled")
            T2.config(state="disabled")
            T4.config(state="disabled")
            T5.config(state="disabled")

        def priorityEN():
            clearBoxes()
            self.adHoc = "todo"
            T4.config(state="normal")
            text.config(state="disabled")
            T1.config(state="disabled")
            T2.config(state="disabled")
            T3.config(state="disabled")
            T5.config(state="disabled")

        def selectEN():
            clearBoxes()
            self.adHoc = "select"
            text.config(state="normal")
            T1.config(state="disabled")
            T2.config(state="disabled")
            T3.config(state="disabled")
            T4.config(state="disabled")
            T5.config(state="disabled")

        self.Rad = IntVar()
        R1 = Radiobutton(queryW, text="Display the items of organizer", variable=self.Rad, value=1, command=organizerEN)
        R1.place(x=10, y=10)
        R1.select()
        self.adHoc = "organizer"

        R2 = Radiobutton(queryW, text="How many events take place at", variable=self.Rad, value=2, command=locationEN)
        R2.place(x=10, y=50)

        R3 = Radiobutton(queryW, text="Display events that take place after", variable=self.Rad, value=3, command=timeEN)
        R3.place(x=10, y=100)

        R4 = Radiobutton(queryW, text="Display the todo items of priority", variable=self.Rad, value=4, command=priorityEN)
        R4.place(x=10, y=150)

        R5 = Radiobutton(queryW, text="How many event and todo items have the summary", variable=self.Rad, value=5, command=selTodo)
        R5.place(x=10, y=200)

        R6 = Radiobutton(queryW, text="", variable=self.Rad, value=6, command=selectEN)
        R6.place(x=10, y=240)

        T1 = Entry(queryW, width=20, state="normal")
        T1.place(x=220, y=10)

        T2 = Entry(queryW, width=20, state="disabled")
        T2.place(x=230, y=50)

        T3 = Entry(queryW, width=11, state="disabled")
        T3.place(x=255, y=100)

        lbl = Label(queryW, text="(YYYY-MM-DD)", font=("Courier", 8, "normal"))
        lbl.place(x=355, y=102)

        T4 = Entry(queryW, width=20, state="disabled")
        T4.place(x=240, y=150)

        T5 = Entry(queryW, width=11, state="disabled")
        T5.place(x=354, y=200)

        def printList(result):
            for x in range(0, len(result)):
                for y in range(0, len(result[x])):
                    text2.insert(INSERT, "%s "%(str(result[x][y])))
                text2.insert(INSERT, "\n")
            text2.insert(INSERT, "-----------------------------------------------------------")
            text2.see(END)
            text2.insert(INSERT, "\n")

        def submitBtn():
            if(self.adHoc is "todo"):
                try:
                    query = "SELECT * FROM TODO WHERE priority = '%s'"%(T4.get())
                    self.cursor.execute(query)
                    result = self.cursor.fetchall()
                    printList(result)
                except mysql.connector.Error as err:
                    message = "Something went wrong: {}".format(err)
                    text2.insert(INSERT, message)
                    text2.see(END)
                    text2.insert(INSERT, "\n")

            elif(self.adHoc is "organizer"):
                try:
                    query = "SELECT org_id from ORGANIZER where name LIKE '%s'"%(T1.get())
                    self.cursor.execute(query)
                    tp = self.cursor.fetchone()
                    if(tp is not None):
                        temp = tp[0]
                        query = "SELECT summary From EVENT WHERE organizer = %s"%(temp)
                        self.cursor.execute(query)
                        result = self.cursor.fetchall()
                        query = "SELECT summary From TODO WHERE organizer = %s"%(temp)
                        self.cursor.execute(query)
                        temp = self.cursor.fetchall()
                        for x in range(0, len(temp)):
                            result.append(temp[x])
                        printList(result)
                    else:
                        text2.insert(INSERT, "0\n")
                        text2.insert(INSERT, "-----------------------------------------------------------")
                        text2.see(END)
                        text2.insert(INSERT, "\n")
                except mysql.connector.Error as err:
                    message = "Something went wrong: {}".format(err)
                    text2.insert(INSERT, message)
                    text2.see(END)
                    text2.insert(INSERT, "\n")

            elif(self.adHoc is "location"):
                try:
                    query = "SELECT COUNT(*) FROM EVENT WHERE location = '%s'"%(T2.get())
                    self.cursor.execute(query)
                    tp = self.cursor.fetchone()
                    if(tp is not None):
                        temp = tp[0]
                        text2.insert(INSERT, str(temp))
                        text2.insert(INSERT, "\n")
                        text2.insert(INSERT, "-----------------------------------------------------------")
                        text2.see(END)
                        text2.insert(INSERT, "\n")
                    else:
                        text2.insert(INSERT, "0\n")
                        text2.insert(INSERT, "-----------------------------------------------------------")
                        text2.see(END)
                        text2.insert(INSERT, "\n")

                except mysql.connector.Error as err:
                    message = "Something went wrong: {}".format(err)
                    text2.insert(INSERT, message)
                    text2.see(END)
                    text2.insert(INSERT, "\n")

            elif(self.adHoc is "time"):
                try:
                    query = "SELECT summary FROM EVENT WHERE start_time > '%s'"%(T3.get())
                    self.cursor.execute(query)
                    result = self.cursor.fetchall()
                    printList(result)
                except mysql.connector.Error as err:
                    message = "Something went wrong: {}".format(err)
                    text2.insert(INSERT, message)
                    text2.see(END)
                    text2.insert(INSERT, "\n")

            elif(self.adHoc is "summary"):
                try:
                    query = "SELECT COUNT(*) From TODO WHERE summary = '%s'"%(T5.get())
                    self.cursor.execute(query)
                    result = self.cursor.fetchone()[0]

                    query = "SELECT COUNT(*) From EVENT WHERE summary = '%s'"%(T5.get())
                    self.cursor.execute(query)
                    result = result + self.cursor.fetchone()[0]
                    text2.insert(INSERT, str(result))
                    text2.insert(INSERT, "\n")
                    text2.insert(INSERT, "-----------------------------------------------------------")
                    text2.see(END)
                    text2.insert(INSERT, "\n")
                except mysql.connector.Error as err:
                    message = "Something went wrong: {}".format(err)
                    text2.insert(INSERT, message)
                    text2.see(END)
                    text2.insert(INSERT, "\n")

            elif(self.adHoc is "select"):
                try:
                    listP = ['DESCRIBE', 'EXPLAIN', 'HELP', 'USE', 'SELECT']
                    self.cursor.execute(text.get(1.0, END))
                    if(text.get(1.0, END).split(' ', 1)[0] in listP):
                        result = self.cursor.fetchall()
                        printList(result)
                    else:
                        text2.insert(INSERT, "Completed\n")
                        query = "SELECT COUNT(*) FROM ORGANIZER"
                        self.cursor.execute(query)
                        org = (self.cursor.fetchone()[0])

                        query = "SELECT COUNT(*) FROM EVENT"
                        self.cursor.execute(query)
                        evt = (self.cursor.fetchone()[0])

                        query = "SELECT COUNT(*) FROM TODO"
                        self.cursor.execute(query)
                        todo = (self.cursor.fetchone()[0])
                        text2.insert(INSERT, "Database has %d organizers, %d events, %d to-do items.\n"%(org, evt, todo) )
                        text2.insert(INSERT, "-----------------------------------------------------------")
                        text2.see(END)
                        text2.insert(INSERT, "\n")
                except mysql.connector.Error as err:
                    message = "Something went wrong: {}".format(err)
                    text2.insert(INSERT, message)
                    text2.see(END)
                    text2.insert(INSERT, "\n")

        def clearTxt():
            text2.delete(1.0, END)

        def clearBoxes():
            text.delete(1.0, END)
            text.insert(INSERT, "SELECT")
            T1.delete(0, 'end')
            T2.delete(0, 'end')
            T3.delete(0, 'end')
            T4.delete(0, 'end')
            T5.delete(0, 'end')

        B1 = Button(queryW, text="Submit", command=submitBtn)
        B1.place(x=170, y=465)

        global Help
        Help = Button(queryW, text="Help", command=self.helpBtn)
        Help.place(x=379, y=465)

        Clear = Button(queryW, text="Clear", command=clearTxt)
        Clear.place(x=379, y=310)

        res = Label(queryW, text="Results:")
        res.place(x=10, y=318)

        def close(event):
            queryW.destroy()

        queryW.bind('<Escape>', close)
        queryW.mainloop()

        queryW.destroy()

    def qExit(self):
        dataBase.entryconfig("Query", state="normal")
        queryW.destroy()

    def helpBtn(self):
        Help.config(state = "disabled")
        helpPop = Tk()
        helpPop.lift()
        helpPop.attributes("-topmost", True)
        helpPop.wm_title("Help")
        helpPop.geometry('340x320+%d+%d' % (helpPop.winfo_screenwidth()/2 +230, helpPop.winfo_screenheight()/2 - 160))
        helpPop.resizable(width=FALSE, height=FALSE)
        helpPop.protocol("WM_DELETE_WINDOW", closer)

        def closer():
            Help.config(state = "normal")
            helpPop.destroy()

        def printTable(result, text):
            for x in range(0, len(result)):
                for y in range(0, len(result[x])):
                    text.insert(INSERT, "%s "%(str(result[x][y])))
                text.insert(INSERT, "\n")

        label = Label(helpPop, text="ORGANIZER TABLE")
        label.place(x=10, y=5)
        Table1 = Text(helpPop, background = "honeydew3", width=44, height = 4)
        Table1.place(x=10,y=20)

        query = "DESCRIBE ORGANIZER;"
        self.cursor.execute(query)
        result = self.cursor.fetchall()
        printTable(result, Table1)
        Table1.config(state="disabled")

        label = Label(helpPop, text="EVENT TABLE")
        label.place(x=10, y=90)
        Table2 = Text(helpPop, background = "honeydew3", width=44, height = 6)
        Table2.place(x=10,y=105)

        query = "DESCRIBE EVENT;"
        self.cursor.execute(query)
        result = self.cursor.fetchall()
        printTable(result, Table2)
        Table2.config(state="disabled")

        label = Label(helpPop, text="TODO TABLE")
        label.place(x=10, y=200)
        Table3 = Text(helpPop, background = "honeydew3", width=44, height = 5)
        Table3.place(x=10,y=215)

        query = "DESCRIBE TODO;"
        self.cursor.execute(query)
        result = self.cursor.fetchall()
        printTable(result, Table3)
        Table3.config(state="disabled")

        B1 = Button(helpPop, text="ok", command=closer)
        B1.place(x=145, y=292)

        def close(event):
            helpPop.destroy()

        helpPop.bind('<Escape>', close)
        helpPop.mainloop()

    # Buttons
    def selected(self):
        self.comp = []
        self.comp.append(int(view.curselection()[0]))
        status = Cal.writeFile( "select.txt", self.result[0], self.comp)
        print(status)
        with open("select.txt", "r") as in_file:
            text = in_file.read()
        os.system("rm -f select.txt")
        log.insert(INSERT, text )
        log.see(END)
        log.insert(INSERT, "\n")

        view.delete(0, END)
        self.result = []
        status = Cal.readFile(self.name, self.result)
        print(status)
        self.count = 0
        self.printFileViewer()
        view.activate(self.comp)
        showSelected.config(state='disabled')
        fileMenu.entryconfig("Save", state="disabled")
        todoMenu.entryconfig("Undo", state="disabled")
        self.undoList = []
        temp = str(self.displayName).split('/')[-1:][0]
        self.parent.title("xcal - %s" % (temp))
        self.unsaved = False

    def extractEvents(self):
        log.insert(INSERT, Popen("cat %s | ./caltool -extract e" % (self.name),
                   shell=True, stdout=PIPE).stdout.read())
        log.see(END)
        log.insert(INSERT, "\n")

    def extractProps(self):
        log.insert(INSERT, Popen("cat %s | ./caltool -extract x" % (self.name),
                   shell=True, stdout=PIPE).stdout.read())
        log.see(END)
        log.insert(INSERT, "\n")

    def clearLog(self):
        log.delete('0.0', END)

    # Destroy Datemsk
    def destroyDatemsk(self):
        dateMsk.destroy()

    # Fromats text for FVP listbox
    def formatList(self, index, name, prop, sub, summary):
        index = " " + index
        template = "{0:5}{1:14}{2:7}{3:8}{4:10}"
        string = template.format(index, name, prop, sub, summary)
        return string

    def printFileViewer(self):
        self.compList = []
        self.count = 0
        for x in range(1, len(self.result)):
            string = self.formatList(str(x), str(self.result[x][0]), str(self.result[x][1]), str(self.result[x][2]), str(self.result[x][3]) )
            view.insert(self.count, string)
            self.compList.append(x)
            self.count = self.count + 1

    # Exit main gui with confirm
    def onExit(self):
        if messagebox.askokcancel("Exit", "Confirm exit?"):
            if(self.result != []):
                Cal.freeFile(self.result[0])
            os.system("rm -f select.txt unSaved.txt err.txt %s" %(self.username))
            if(len(sys.argv) > 1):
                self.db.close()
            self.quit()
            quit()

    def saveFile(self):
        self.name = self.displayName
        status = Cal.writeFile( self.name, self.result[0], self.compList)
        self.storeSQL = []
        Cal.storeFile(self.name, self.storeSQL)
        log.insert(INSERT, status )
        log.see(END)
        log.insert(INSERT, "\n")
        fileMenu.entryconfig("Save", state="disabled")
        todoMenu.entryconfig("Undo", state="disabled")
        self.undoList = []
        temp = str(self.displayName).split('/')[-1:][0]
        self.parent.title("xcal - %s" % (temp))
        self.unsaved = False

    def saveFileAs(self):
        temp = asksaveasfilename(defaultextension='.ics', filetypes=[("Calender files","*.ics"),("all files","*.*")])

        if(temp or temp is not None):
            self.displayName = temp
            temp = str(self.displayName).split('/')[-1:][0]
            self.parent.title("xcal - %s" % (temp))
            self.name = self.displayName
            status = Cal.writeFile( self.name, self.result[0], self.compList)
            self.storeSQL = []
            Cal.storeFile(self.name, self.storeSQL)
            log.insert(INSERT, status )
            log.see(END)
            log.insert(INSERT, "\n")

            fileMenu.entryconfig("Save", state="disabled")
            todoMenu.entryconfig("Undo", state="disabled")
            self.undoList = []
            self.unsaved = False

    def clearList(self):
        replace.destroy()
        self.name = ""
        self.callback()

    def Replace(self):
        global replace
        replace = Toplevel()
        replace.attributes("-topmost", True)
        replace.wm_title("File Already Open")
        replace.geometry('250x120+%d+%d' % (replace.winfo_screenwidth()/2 - 125, replace.winfo_screenheight()/2 - 60))
        replace.resizable(width=FALSE, height=FALSE)

        label = Label(replace, text="Warning: All changes will be discarded")
        label.pack(side="top", fill="x", pady=10)

        B1 = Button(replace, text="OK", command=self.clearList)
        B1.place(x=60, y=60)

        B2 = Button(replace, text="Cancel", command=replace.destroy)
        B2.place(x=125, y=60)

        def close(event):
            replace.destroy()

        replace.bind('<Escape>', close)
        replace.mainloop()

    # Gets clalender file for open
    def callback(self):
        if(self.name == "" or not self.name or self.unSaved is False):
            self.unSaved = False
            self.displayName = askopenfilename(filetypes=(("Calendar files", "*.ics"),
                                  ("text files", "*.txt"),
                                  ("All files", "*.*")))

            if(not self.displayName or self.displayName == "()"):
                self.server = False

            elif(self.displayName or self.displayName != "()" or self.displayName is not None):
                self.name = self.displayName
                text = Popen('cat %s | ./caltool -info 2> err.txt'%(self.name), stdout=PIPE, shell=True).stdout.read()
                if(os.stat("err.txt").st_size != 0):
                    with open('err.txt', 'r') as myfile:
                        text=myfile.read()
                    log.insert(INSERT, "File: %s\n"%(self.name.split('/')[-1:][0]))
                    log.insert(INSERT, text)
                    log.see(END)
                    log.insert(INSERT, "\n")
                else:
                    log.insert(INSERT, text)
                    log.see(END)
                    log.insert(INSERT, "\n")
                    if(self.username != ""):
                        daveMenu.entryconfig("Export", state="normal")
                    os.system("rm -f %s.ics"%(self.username))
                    self.unSaved = False
                    view.delete(0, END)
                    temp = str(self.name).split('/')[-1:][0]
                    self.parent.title("xcal - %s" % (temp))
                    extractElements.config(state=NORMAL)
                    extractProps.config(state=NORMAL)

                    self.result = []
                    status = Cal.readFile(self.name, self.result)
                    self.printFileViewer()

                    self.storeSQL = []
                    Cal.storeFile(self.name, self.storeSQL)

                    fileMenu.entryconfig("Combine", state="normal")
                    todoMenu.entryconfig("To-do List", state="normal")
                    fileMenu.entryconfig("Save as", state = "normal")
                    dataBase.entryconfig("Store All", state = "normal")
                    self.filterFile = True
                    if(self.filterDate is True and self.filterFile is True):
                        fileMenu.entryconfig("Filter", state="normal")
        else:
            self.Replace()

    # gets datemsk from menubar
    def getDateMsk(self):
        dateMsk.destroy()
        temp = askopenfilename(filetypes=(("text files", "*.txt"),
                               ("All files", "*.*")))
        if(not temp or temp == "()"):
            self.server = False
        elif(temp or temp is not None):
            os.putenv('DATEMSK', temp)
            self.filterDate = True
            if(self.filterDate is True and self.filterFile is True):
                fileMenu.entryconfig("Filter", state="normal")

    # Gets clalender file for combine
    def Combine(self):
        self.combineName = askopenfilename(filetypes=(("Calendar files", "*.ics"),
                                    ("text files", "*.txt"),
                                    ("All files", "*.*")))
        if(self.combineName):
            os.system("cat %s | ./caltool -combine %s > unSaved.txt 2> err.txt" % (self.name, self.combineName))
            if(os.stat("err.txt").st_size != 0):
                with open('err.txt', 'r') as myfile:
                    text=myfile.read()
                log.insert(INSERT, "File: %s\n"%(self.combineName.split('/')[-1:][0]))
                log.insert(INSERT, text)
                log.see(END)
                log.insert(INSERT, "\n")
            else:
                temp = str(self.displayName).split('/')[-1:][0]
                self.unSaved = True
                fileMenu.entryconfig("Save", state="normal")
                self.parent.title("xcal - %s" % (temp+"*"))
                view.delete(0 , END)
                self.result = []
                Cal.readFile("unSaved.txt", self.result)
                self.printFileViewer()
                self.storeSQL = []
                Cal.storeFile("unSaved.txt", self.storeSQL)

    # Filter Window
    def getFilter(self):
        global wdw
        wdw = Toplevel()
        wdw.attributes("-topmost", True)
        wdw.geometry('600x150+%d+%d' % (self.winfo_screenwidth()/2-300, self.winfo_screenheight()/2-75))
        wdw.resizable(width=FALSE, height=FALSE)
        wdw.title("Filter")

        filterBtn = Button(wdw, text="Filter", command=self.filtering, state=DISABLED)
        filterBtn.place(x=225, y=100)
        cancelBtn = Button(wdw, text="Cancel", command=wdw.destroy)
        cancelBtn.place(x=300, y=100)

        global event
        global fromE
        global toE

        def selTodo():
            filterBtn.config(state='normal')
            global eventRad
            eventRad = event.get()
            fromE.config(state='disabled')
            toE.config(state='disabled')

        def selEvent():
            filterBtn.config(state='normal')
            global eventRad
            eventRad = event.get()
            fromE.config(state='normal')
            toE.config(state='normal')

        def close(event):
            wdw.destroy()

        wdw.bind('<Escape>', close)

        event = IntVar()
        R1 = Radiobutton(wdw, text="To-do Items", variable=event, value=1, command=selTodo)
        R1.place(x=10, y=15)

        R2 = Radiobutton(wdw, text="Events", variable=event, value=2, command=selEvent)
        R2.place(x=10, y=45)

        fromLabel = Label(wdw, text="From")
        fromLabel.place(x=210, y=18)
        fromE = Entry(wdw, width=20, state=DISABLED)
        fromE.place(x=140, y=40)

        toLabel = Label(wdw, text="To")
        toLabel.place(x=440, y=18)
        toE = Entry(wdw, width=20, state=DISABLED)
        toE.place(x=360, y=40)

        wdw.transient(self)
        wdw.grab_set()
        self.wait_window(wdw)

    def filtering(self):
        if(eventRad == 1):
            text = Popen("cat %s | ./caltool -filter t 2> err.txt | ./caltool -info" %(self.name),
                         shell=True, stdout=PIPE).stdout.read()
            if(os.stat("err.txt").st_size != 0):
                with open('err.txt', 'r') as myfile:
                    text=myfile.read()
                log.insert(INSERT, text)
                log.see(END)
            else:
                self.unSaved = True
                fileMenu.entryconfig("Save", state="normal")
                self.parent.title("xcal - %s" % (self.displayName.split('/')[-1:][0] +"*"))
                log.insert(INSERT, text )
                log.see(END)
                os.system("cat %s | ./caltool -filter t > unSaved.txt" %(self.name))
                view.delete(0 , END)
                self.result = []
                Cal.readFile("unSaved.txt", self.result)
                self.printFileViewer()
                self.storeSQL = []
                Cal.storeFile("unSaved.txt", self.storeSQL)

        elif(eventRad == 2):
            if(fromE.get() == "" and toE.get() == "" ):
                text = Popen("cat %s | ./caltool -filter e 2> err.txt | ./caltool -info" %(self.name),
                           shell=True, stdout=PIPE).stdout.read()
                if(os.stat("err.txt").st_size != 0):
                    with open('err.txt', 'r') as myfile:
                        text=myfile.read()
                    log.insert(INSERT, text)
                    log.see(END)
                else:
                    self.unSaved = True
                    fileMenu.entryconfig("Save", state="normal")
                    self.parent.title("xcal - %s" % (self.displayName.split('/')[-1:][0] +"*"))
                    log.insert(INSERT, text )
                    log.see(END)
                    os.system("cat %s | ./caltool -filter e > unSaved.txt" %(self.name))
                    view.delete(0 , END)
                    self.result = []
                    Cal.readFile("unSaved.txt", self.result)
                    self.printFileViewer()
                    self.storeSQL = []
                    Cal.storeFile("unSaved.txt", self.storeSQL)

            elif( fromE.get() != "" and toE.get() == ""  ):
                text = Popen('cat %s | ./caltool -filter e from "%s" 2> err.txt | ./caltool -info' %(self.name, fromE.get()),
                           shell=True, stdout=PIPE).stdout.read()
                if(os.stat("err.txt").st_size != 0):
                    with open('err.txt', 'r') as myfile:
                        text=myfile.read()
                    log.insert(INSERT, text)
                    log.see(END)
                else:
                    self.unSaved = True
                    fileMenu.entryconfig("Save", state="normal")
                    self.parent.title("xcal - %s" % (self.displayName.split('/')[-1:][0] +"*"))
                    log.insert(INSERT, text )
                    log.see(END)
                    os.system('cat %s | ./caltool -filter e from "%s" > unSaved.txt' %(self.name, fromE.get()))
                    view.delete(0 , END)
                    self.result = []
                    Cal.readFile("unSaved.txt", self.result)
                    self.printFileViewer()
                    self.storeSQL = []
                    Cal.storeFile("unSaved.txt", self.storeSQL)

            elif( fromE.get() == "" and toE.get() != "" ):
                text = Popen('cat %s | ./caltool -filter e to "%s" 2> err.txt | ./caltool -info' %(self.name, toE.get()),
                           shell=True, stdout=PIPE).stdout.read()
                if(os.stat("err.txt").st_size != 0):
                    with open('err.txt', 'r') as myfile:
                        text=myfile.read()
                    log.insert(INSERT, text)
                    log.see(END)
                else:
                    self.unSaved = True
                    fileMenu.entryconfig("Save", state="normal")
                    self.parent.title("xcal - %s" % (self.displayName.split('/')[-1:][0] +"*"))
                    log.insert(INSERT, text )
                    log.see(END)
                    os.system('cat %s | ./caltool -filter e to "%s" > unSaved.txt' %(self.name, toE.get()))
                    view.delete(0 , END)
                    self.result = []
                    Cal.readFile("unSaved.txt", self.result)
                    self.printFileViewer()
                    self.storeSQL = []
                    Cal.storeFile("unSaved.txt", self.storeSQL)

            elif( fromE.get() != "" and toE.get() != "" ):
                text = Popen('cat %s | ./caltool -filter e from "%s" to "%s" 2> err.txt | ./caltool -info' %(self.name, fromE.get(), toE.get()),
                           shell=True, stdout=PIPE).stdout.read()
                if(os.stat("err.txt").st_size != 0):
                    with open('err.txt', 'r') as myfile:
                        text=myfile.read()
                    log.insert(INSERT, text)
                    log.see(END)
                else:
                    self.unSaved = True
                    fileMenu.entryconfig("Save", state="normal")
                    self.parent.title("xcal - %s" % (self.displayName +"*"))
                    log.insert(INSERT, text )
                    log.see(END)
                    os.system('cat %s | ./caltool -filter e from "%s" to "%s" > unSaved.txt' %(self.name, fromE.get(), toE.get()))
                    view.delete(0 , END)
                    self.result = []
                    Cal.readFile("unSaved.txt", self.result)
                    self.printFileViewer()
                    self.storeSQL = []
                    Cal.storeFile("unSaved.txt", self.storeSQL)

        log.insert(INSERT, "\n")
        wdw.destroy()

    def Todo(self):
        global todo
        todo = Toplevel()
        todo.attributes("-topmost", True)
        todo.geometry('300x300+%d+%d' % (self.winfo_screenwidth()/2-150, self.winfo_screenheight()/2-150))
        todo.resizable(width=FALSE, height=FALSE)
        todo.title("To-do List")

        todoFrame = Frame(todo, height=240, width=300)
        todoFrame.pack()

        scrollbar = Scrollbar(todoFrame)
        scrollbar.pack(side=RIGHT, fill=Y)

        todoList = CheckList(todoFrame)
        todoList.hlist.config(selectforeground="black", height=16, width=45, yscrollcommand=scrollbar.set)
        todoList.pack()

        scrollbar.config(command=view.yview)

        def Done():
            checked = list(map(int, todoList.getselection("on")))

            if(len(checked) > 0):
                self.unsaved = True
                todoMenu.entryconfig("Undo", state="normal")
                temp = str(self.displayName).split('/')[-1:][0]
                self.parent.title("xcal - %s" % (temp+"*"))
                view.delete(0, END)

                if(self.todoInit is False):
                    self.undoList = self.result
                    self.todoInit = True

                counter = 0
                new = []
                new.append(self.result[0])
                for x in range(1, len(self.result)):
                    if((self.result[x][0]) == "VTODO"):
                        if(counter not in checked):
                            new.append(self.result[x])
                            self.compList.append(x)
                        counter = counter + 1
                    else:
                        new.append(self.result[x])
                        self.compList.append(x)

                fileMenu.entryconfig("Save", state="normal")
                fileMenu.entryconfig("Save as", state="normal")
                self.result = new
                self.printFileViewer()

            todo.destroy()

        doneBtn = Button(todo, text="Done", command=Done, state=NORMAL)
        doneBtn.place(x=75, y=260)
        cancelBtn = Button(todo, text="Cancel", command=todo.destroy)
        cancelBtn.place(x=160, y=260)

        counter = 0
        for x in range(1, len(self.result)):
            if((self.result[x][0]) == "VTODO"):
                string = str(self.result[x][3])
                todoList.hlist.add(str(counter), text=string)
                todoList.setstatus(str(counter), "off")
                counter = counter + 1

        def close(event):
            todo.destroy()

        todo.bind('<Escape>', close)

        todo.mainloop()

    def Undo(self):
        global undo
        undo = Toplevel()
        undo.attributes("-topmost", True)
        undo.wm_title("Undo")
        undo.geometry('420x120+%d+%d' % (undo.winfo_screenwidth()/2 - 210, undo.winfo_screenheight()/2 - 60))
        undo.resizable(width=FALSE, height=FALSE)

        label = Label(undo, text="Warning: All Todo components since last save will be restored")
        label.pack(side="top", fill="x", pady=10)

        def undoCmd():
            self.unsaved = False
            todoMenu.entryconfig("Undo", state="disabled")
            temp = str(self.displayName).split('/')[-1:][0]
            self.parent.title("xcal - %s" % (temp))
            view.delete(0 , END)

            self.result = self.undoList
            self.printFileViewer()
            undo.destroy()


        B1 = Button(undo, text="Undo", command=undoCmd)
        B1.place(x=135, y=60)

        B2 = Button(undo, text="Cancel", command=undo.destroy)
        B2.place(x=235, y=60)

        def close(event):
            undo.destroy()

        undo.bind('<Escape>', close)
        undo.mainloop()

    def accountPrompt(self):
        account = Toplevel()
        account.attributes("-topmost", True)
        account.wm_title("Account")
        account.geometry('250x120+%d+%d' % (account.winfo_screenwidth()/2 - 125, account.winfo_screenheight()/2 - 60))
        account.resizable(width=FALSE, height=FALSE)

        label = Label(account, text="userid")
        label.place(x=24,y=15)

        label = Label(account, text="password")
        label.place(x=5, y=35)

        name = Entry(account, width=17)
        name.place(x=80, y=15)

        password = Entry(account, width=17, show="*")
        password.place(x=80, y=35)

        def signIn():
            self.username = name.get()
            self.passw = password.get()

            self.url = 'https://calendar.socs.uoguelph.ca/%s/'%(self.username)

            r = requests.get(self.url, auth=(self.username, self.passw))

            if r.status_code == 200:
                log.insert(INSERT, "Successfully logged in as %s\n"%(self.username))
                log.see(END)
                log.insert(INSERT, "\n")
                daveMenu.entryconfig("Import", state="normal")
                if(self.name != ""):
                    daveMenu.entryconfig("Export", state="normal")
            else:
                log.insert(INSERT, "username and/or password is incorrect\n")
                log.see(END)
                log.insert(INSERT, "\n")

                with open(self.name, 'rb') as fh:
                    r = requests.put(self.url, data=fh, auth=(self.username, self.passw))
            account.destroy()

        B1 = Button(account, text="OK", command=signIn)
        B1.place(x=50, y=75)

        B2 = Button(account, text="Cancel", command=account.destroy)
        B2.place(x=130, y=75)

        def close(event):
            account.destroy()

        account.bind('<Escape>', close)

        account.transient(self)
        account.grab_set()
        self.wait_window(account)

    def Import(self):
        self.server = True
        r = requests.get(self.url, auth=(self.username, self.passw))
        self.name = os.getcwd() + '/%s.ics' %(self.username)
        f = open(self.name, 'w+')
        f.write(r.text)
        f.close()

        text = Popen('cat %s | ./caltool -info 2> err.txt'%(self.name), stdout=PIPE, shell=True).stdout.read()
        if(os.stat("err.txt").st_size != 0):
            with open('err.txt', 'r') as myfile:
                text=myfile.read()
            log.insert(INSERT, text)
            log.see(END)
            log.insert(INSERT, "\n")
        else:
            self.displayName = self.username
            log.insert(INSERT, text)
            log.see(END)
            log.insert(INSERT, "\n")
            self.server = True
            self.unSaved = False
            view.delete(0, END)
            self.parent.title("xcal - %s" % (self.displayName))
            extractElements.config(state=NORMAL)
            extractProps.config(state=NORMAL)

            self.result = []
            status = Cal.readFile(self.name, self.result)
            self.printFileViewer()

            daveMenu.entryconfig("Export", state="normal")
            fileMenu.entryconfig("Combine", state="normal")
            todoMenu.entryconfig("To-do List", state="normal")
            fileMenu.entryconfig("Save as", state = "normal")
            self.filterFile = True
            if(self.filterDate is True and self.filterFile is True):
                fileMenu.entryconfig("Filter", state="normal")
            self.parent.title("xcal - %s" % (self.username))

    def Export(self):
        old = ""
        if(self.unSaved is True):
            self.name = os.getcwd() + '/%s.ics' %(self.username)
            status = Cal.writeFile( self.name, self.result[0], self.compList)
            log.insert(INSERT, status )
            log.see(END)
            log.insert(INSERT, "\n")

        r = requests.delete(self.url, auth=(self.username, self.passw))
        count = 0
        with open(self.name, 'rb') as fh:
            r = requests.put(self.url, data=fh, auth=(self.username, self.passw))
            count = count + 1

        text = "Successfully wrote %s to %s\n" %(self.name.split('/')[-1:][0], self.username)
        log.insert(INSERT, text)
        log.see(END)
        log.insert(INSERT, "\n")


    # About xcal window
    def About(self):
        popup = Tk()
        popup.lift()
        popup.attributes("-topmost", True)
        popup.wm_title("About")
        popup.geometry('300x150+%d+%d' % (popup.winfo_screenwidth()/2 - 150, popup.winfo_screenheight()/2 - 75))
        popup.resizable(width=FALSE, height=FALSE)

        label = Label(popup, text="App Name: xcal")
        label.pack(side="top", fill="x", pady=5)
        label = Label(popup, text="Created by: Nick Major")
        label.pack(side="top", fill="x", pady=5)
        label = Label(popup, text="xcal is compatible with iCalendar V2.0.")
        label.pack(side="top", fill="x", pady=20)
        B1 = Button(popup, text="ok", command=popup.destroy)
        B1.pack()

        def close(event):
            popup.destroy()

        popup.bind('<Escape>', close)
        popup.mainloop()


# Main loop
def main():
    global root
    root = Tk()
    root.geometry('655x570+%d+%d' % (root.winfo_screenwidth()/2 - 327, root.winfo_screenheight()/2 - 282))
    root.minsize(655, 570)
    root.app = xcal(root)
    root.mainloop()

if __name__ == "__main__":
    main()
